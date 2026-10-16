#include "usart.h"
#include "uart_port.h"
#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
UART_HandleTypeDef huart1;
uint32_t mock_primask, mock_ipsr, mock_basepri, mock_faultmask, mock_tc=1, mock_rxne;
static uint32_t now,tick_step;
static unsigned assertions,rx_fail,tx_fail,abort_fail;
static uint8_t *rx_target, *tx_target;
static uint16_t sent_size;
static int rx_busy,tx_busy;
static int inject_error_before_lock;
#define CHECK(x) do { assertions++; if (!(x)) { fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#x); exit(1); } } while(0)
uint32_t HAL_GetTick(void) { uint32_t n=now; now+=tick_step; return n; }
void Mock_DisableIRQ(void)
{
    /* 模拟关中断指令前被UART IRQ抢占，同时SysTick越过1ms边界。 */
    if (inject_error_before_lock && !mock_primask) {
        inject_error_before_lock=0;
        now++;
        huart1.ErrorCode=HAL_UART_ERROR_FE;
        mock_ipsr=37;
        HAL_UART_ErrorCallback(&huart1);
        mock_ipsr=0;
        huart1.ErrorCode=0;
    }
    mock_primask=1U;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *b,uint16_t n)
{
    CHECK(u==&huart1 && n==1 && b!=NULL);
    if (rx_fail) { rx_fail--; return HAL_BUSY; }
    if (rx_busy) { return HAL_BUSY; }
    u->ErrorCode=0; rx_target=b; rx_busy=1; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *u,uint8_t *b,uint16_t n)
{
    CHECK(u==&huart1 && n && n<=256);
    if (tx_fail) { tx_fail--; return HAL_ERROR; }
    if (tx_busy) { return HAL_BUSY; }
    tx_target=b;sent_size=n;tx_busy=1;mock_tc=0;return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u)
{
    CHECK(u==&huart1);
    if (abort_fail) { abort_fail--; return HAL_ERROR; }
    rx_busy=0; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u)
{
    CHECK(u==&huart1);tx_busy=0;return HAL_OK;
}
static void rx(uint8_t b, uint32_t err)
{
    CHECK(rx_busy);
    *rx_target=b; rx_busy=0; huart1.ErrorCode=err; mock_ipsr=37;
    HAL_UART_RxCpltCallback(&huart1);
    if (err) { HAL_UART_ErrorCallback(&huart1); huart1.ErrorCode=0; }
    mock_ipsr=0;
}
static void done(void)
{
    tx_busy=0;mock_ipsr=37;HAL_UART_TxCpltCallback(&huart1);mock_ipsr=0;mock_tc=1;
    Uart_Service();
}
static void advance(void) { now+=5; Uart_Service(); }
int main(void)
{
    RingBuffer r;
    UartStats s;
    uint8_t b,buf[256],original[256];
    uint32_t bits,seed=0x3C5F1932U,model_head=0,model_count=0;
    uint8_t model[2048];
    unsigned i,j;
    Ring_Reset(&r);CHECK(!Ring_Pop(&r,&b));CHECK(!Ring_Pop(&r,NULL));
    for(i=0;i<2047;i++) {CHECK(Ring_Push(&r,(uint8_t)i));}
    CHECK(Ring_Count(&r)==2047);CHECK(!Ring_Push(&r,99));
    for(i=0;i<2047;i++) {CHECK(Ring_Pop(&r,&b));CHECK(b==(uint8_t)i);}
    /* 独立FIFO oracle：百万次随机交错覆盖head/tail多次回绕和0..2047占用。 */
    for(i=0;i<1000000;i++) {
        seed=seed*1664525U+1013904223U;
        if (seed&0x80000000U) {
            b=(uint8_t)(seed>>16);
            CHECK(Ring_Push(&r,b)==(model_count<2047));
            if(model_count<2047) { model[(model_head+model_count)%2048]=b;model_count++; }
        } else {
            int ok=Ring_Pop(&r,&b);CHECK(ok==(model_count!=0));
            if(model_count) { CHECK(b==model[model_head]);model_head=(model_head+1)%2048;model_count--; }
        }
        CHECK(Ring_Count(&r)==model_count);
    }
    CHECK(!Uart_ReadByte(&b));CHECK(!Uart_TxSubmit(&b,1));
    mock_primask=1;CHECK(!Uart_Start());mock_primask=0;
    mock_basepri=0x50;CHECK(!Uart_Start());mock_basepri=0;
    mock_faultmask=1;CHECK(!Uart_Start());mock_faultmask=0;
    rx_fail=1;CHECK(!Uart_Start());CHECK(!rx_busy);
    bits=Uart_TakeErrors();CHECK(bits==UART_ERR_RX_START);advance();CHECK(rx_busy);
    CHECK(!Uart_Start());
    for(j=0;j<200;j++) {
        for(i=0;i<257;i++) {rx((uint8_t)(i+j),0);}
        for(i=0;i<257;i++) {CHECK(Uart_ReadByte(&b));CHECK(b==(uint8_t)(i+j));}
        CHECK(!Uart_ReadByte(&b));
    }
    for(i=0;i<2048;i++) {rx((uint8_t)i,0);}
    CHECK(!Uart_ReadByte(&b));CHECK(!rx_busy);
    Uart_GetStats(&s);CHECK(s.queued_bytes==0 && s.discarded_bytes==2048);
    advance();CHECK(rx_busy);rx(0xA1,0);CHECK(!Uart_ReadByte(&b));
    CHECK(Uart_TakeErrors()==UART_ERR_OVERFLOW);CHECK(Uart_ReadByte(&b)&&b==0xA1);
    for(i=0;i<4;i++) {
        static const uint32_t halbits[]={HAL_UART_ERROR_ORE,HAL_UART_ERROR_FE,HAL_UART_ERROR_NE,HAL_UART_ERROR_PE};
        static const uint32_t expect[]={UART_ERR_ORE,UART_ERR_FE,UART_ERR_NE,UART_ERR_PE};
        rx(55,0);rx(66,halbits[i]);CHECK(!Uart_ReadByte(&b));CHECK(!rx_busy);
        CHECK(Uart_TakeErrors()==expect[i]);advance();CHECK(rx_busy);
        rx(77,0);CHECK(Uart_ReadByte(&b)&&b==77);CHECK(!Uart_ReadByte(&b));
    }
    rx(9,HAL_UART_ERROR_ORE|HAL_UART_ERROR_NE|HAL_UART_ERROR_FE);
    CHECK(Uart_TakeErrors()==(UART_ERR_ORE|UART_ERR_NE|UART_ERR_FE));
    rx_fail=1;advance();CHECK(!rx_busy);CHECK(Uart_TakeErrors()==UART_ERR_RX_START);advance();CHECK(rx_busy);
    rx_fail=1;rx(1,0);CHECK(!Uart_ReadByte(&b));CHECK(Uart_TakeErrors()==UART_ERR_RX_START);advance();CHECK(rx_busy);CHECK(!Uart_ReadByte(&b));
    abort_fail=1;rx(7,HAL_UART_ERROR_FE);CHECK(Uart_TakeErrors()==(UART_ERR_FE|UART_ERR_ABORT));advance();
    /* 残留SR/DR不依赖RxCplt已读取DR：Error-only路径也须可恢复。 */
    huart1.ErrorCode=HAL_UART_ERROR_ORE;mock_ipsr=37;HAL_UART_ErrorCallback(&huart1);mock_ipsr=0;
    mock_rxne=1;CHECK(Uart_TakeErrors()==UART_ERR_ORE);advance();CHECK(mock_rxne==0 && rx_busy);
    rx(0xCE,0);CHECK(Uart_ReadByte(&b)&&b==0xCE);
    /* 恢复延时的时序负例：旧now不能与ISR刚写的新rx_retry混算。
       既覆盖普通1ms推进，也覆盖FFFFFFFF->0的自然回绕。 */
    for(i=0;i<2;i++) {
        now=i ? 0xFFFFFFFFU : 100U;
        inject_error_before_lock=1;
        Uart_Service();
        CHECK(!rx_busy);CHECK(Uart_TakeErrors()==UART_ERR_FE);
        now+=4U;Uart_Service();CHECK(!rx_busy);
        now++;Uart_Service();CHECK(rx_busy);
        rx(0xC7,0);CHECK(Uart_ReadByte(&b)&&b==0xC7);
    }
    for(i=0;i<256;i++) {buf[i]=(uint8_t)i;original[i]=(uint8_t)i;}
    CHECK(!Uart_TxSubmit(NULL,1));CHECK(!Uart_TxSubmit(buf,0));CHECK(!Uart_TxSubmit(buf,257));
    mock_ipsr=37;CHECK(!Uart_TxSubmit(buf,1));CHECK(!Uart_FlushTC(1));mock_ipsr=0;
    CHECK(Uart_TxSubmit(buf,256));CHECK(sent_size==256);memset(buf,0,256);
    CHECK(memcmp(tx_target,original,256)==0);CHECK(!Uart_TxSubmit(buf,1));
    /* TC=1而回调未到、回调到而TC=0，两种情况都不能释放静态缓冲。 */
    mock_tc=1;Uart_Service();CHECK(!Uart_TxComplete());mock_tc=0;
    HAL_UART_TxCpltCallback(&huart1);Uart_Service();CHECK(!Uart_TxComplete());
    CHECK(memcmp(tx_target,original,256)==0);done();CHECK(Uart_TxComplete());CHECK(Uart_FlushTC(1));
    CHECK(!Uart_FlushTC(0));CHECK(!Uart_FlushTC(1001));
    tx_fail=1;CHECK(!Uart_TxSubmit(buf,1));CHECK(Uart_TakeErrors()==UART_ERR_TX_START);CHECK(!Uart_TxComplete());
    CHECK(Uart_TxSubmit(buf,1));done();CHECK(Uart_TxComplete());
    now=0xFFFFFFF0U;CHECK(Uart_TxSubmit(buf,1));now+=99;Uart_Service();CHECK(!Uart_TxComplete());now++;Uart_Service();
    CHECK(Uart_TakeErrors()==UART_ERR_TX_TIMEOUT);CHECK(!Uart_TxComplete());
    CHECK(Uart_TxSubmit(buf,1));done();CHECK(Uart_TxComplete());
    CHECK(Uart_TxSubmit(buf,1));tick_step=1;CHECK(!Uart_FlushTC(5));tick_step=0;CHECK(Uart_TakeErrors()==UART_ERR_TX_TIMEOUT);
    CHECK(Uart_TxSubmit(buf,1));done();CHECK(Uart_TxComplete());
    Uart_GetStats(&s);CHECK(s.transmitted_bytes==259);CHECK(s.recoveries>=9);CHECK(mock_primask==0);
    printf("{\"result\":\"PASS\",\"assertions\":%u,\"random_ring_operations\":1000000}\n",assertions);
    return 0;
}

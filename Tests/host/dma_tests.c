#include "usart.h"
#include "uart_port.h"
#include "dma_cursor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
DMA_HandleTypeDef dma;
UART_HandleTypeDef huart1={0,&dma};
uint32_t mock_primask,mock_ipsr,mock_basepri,mock_faultmask,mock_tc=1,mock_rxne,mock_cycles;
static uint32_t now,seed=1234567;
static unsigned checks,fail_rx,fail_tx,fail_abort_rx,fail_abort_tx,rx_starts;
static uint8_t *rx,*tx;
static uint16_t pos;
#define C(x) do {checks++;if(!(x)){fprintf(stderr,"line %d %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t HAL_GetTick(void){return now;}
void Mock_DisableIRQ(void){mock_primask=1;}
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{C(u==&huart1 && n==512);rx_starts++;if(fail_rx){fail_rx--;return HAL_ERROR;}rx=p;pos=0;dma.counter=512;u->ErrorCode=0;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{C(u==&huart1 && n>0 && n<=256);if(fail_tx){fail_tx--;return HAL_ERROR;}tx=p;mock_tc=0;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u){C(u==&huart1);if(fail_abort_rx){fail_abort_rx--;return HAL_TIMEOUT;}return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u){C(u==&huart1);if(fail_abort_tx){fail_abort_tx--;return HAL_TIMEOUT;}return HAL_OK;}
static unsigned rnd(void){seed=seed*1664525U+1013904223U;return seed;}
static void event(void){mock_ipsr=37;HAL_UARTEx_RxEventCallback(&huart1,99);mock_ipsr=0;}
static void feed(unsigned n,uint8_t start){unsigned j;for(j=0;j<n;j++){rx[pos]=(uint8_t)(start+j);pos=(pos+1)&511;dma.counter=512-pos;}event();}
static void recover(void){now+=5;mock_cycles+=360000;Uart_Service();}
int main(void)
{
    unsigned i,j,n;uint8_t b,data[256];uint16_t last=0,d;
    UartStats s;
    C(!DmaCursor_Advance(&last,513,&d));C(!DmaCursor_Advance(NULL,0,&d));
    C(DmaCursor_Advance(&last,256,&d)&&d==256);C(DmaCursor_Advance(&last,0,&d)&&d==256);
    C(DmaCursor_Advance(&last,512,&d)&&d==0);C(!DmaCursor_Advance(&last,255,&d));
    fail_rx=1;C(!Uart_Start());C(Uart_TakeErrors()==UART_ERR_RX_START);recover();
    /* 10000组真实C随机边界，独立逐字节oracle，CNDTR而非callback Size。 */
    for(i=0;i<10000;i++){
        n=rnd()%257;Uart_Service();feed(n,(uint8_t)i);event();event();
        for(j=0;j<n;j++){C(Uart_ReadByte(&b));C(b==(uint8_t)(i+j));}
        C(!Uart_ReadByte(&b));now++;mock_cycles+=72000;
    }
    for(i=0;i<8;i++){feed(256,0);}C(Uart_TakeErrors()==UART_ERR_OVERFLOW);C(!Uart_ReadByte(&b));recover();
    feed(257,0);C(Uart_TakeErrors()==UART_ERR_RX_OVERRUN);recover();
    mock_cycles+=1440000;event();C(Uart_TakeErrors()==UART_ERR_RX_OVERRUN);recover();
    /* HAL tick不走也由CPU周期守卫失败关闭；tick/CPU32位回绕分别覆盖。 */
    mock_cycles=0xFFFFFF00;Uart_Service();(void)Uart_TakeErrors();recover();
    mock_cycles+=72000;Uart_Service();feed(1,77);C(Uart_ReadByte(&b)&&b==77);
    {static const uint32_t bits[]={HAL_UART_ERROR_ORE,HAL_UART_ERROR_FE,HAL_UART_ERROR_NE,HAL_UART_ERROR_DMA};
     static const uint32_t errors[]={UART_ERR_ORE,UART_ERR_FE,UART_ERR_NE,UART_ERR_DMA};
     for(i=0;i<4;i++){huart1.ErrorCode=bits[i];HAL_UART_ErrorCallback(&huart1);C(Uart_TakeErrors()==errors[i]);recover();}}
    memset(data,0xA7,sizeof(data));C(Uart_TxSubmit(data,256));memset(data,0,sizeof(data));C(tx[0]==0xA7);
    C(!Uart_TxSubmit(data,1));mock_tc=1;C(!Uart_TxComplete());mock_tc=0;HAL_UART_TxCpltCallback(&huart1);C(!Uart_TxComplete());mock_tc=1;C(Uart_TxComplete());
    now=0xFFFFFFFA;C(Uart_TxSubmit(data,1));now+=100;Uart_Service();C(!Uart_TxComplete());C(Uart_TakeErrors()&UART_ERR_TX_TIMEOUT);
    fail_tx=1;C(!Uart_TxSubmit(data,1));C(Uart_TakeErrors()&UART_ERR_TX_START);
    C(!Uart_TxComplete() && !Uart_TxBusy());C(Uart_TxSubmit(data,1));C(Uart_TxBusy());mock_tc=1;HAL_UART_TxCpltCallback(&huart1);C(Uart_TxComplete());
    C(Uart_TxSubmit(data,1));huart1.ErrorCode=HAL_UART_ERROR_DMA;HAL_UART_ErrorCallback(&huart1);mock_tc=1;C(!Uart_TxComplete());C(Uart_TakeErrors()&UART_ERR_DMA);recover();
    Uart_GetStats(&s);C(s.queued_bytes==0 && s.recoveries>0);
    feed(50,8);C(Uart_RxFence());C(!Uart_ReadByte(&b));feed(1,42);C(Uart_ReadByte(&b) && b==42);
    /* 中止失败不是完成：禁止复用仍可能被DMA引用的TX缓冲；迟到TC也不能报成功。 */
    memset(data,0xA7,sizeof(data));C(Uart_TxSubmit(data,256));fail_abort_tx=3;
    now+=100;Uart_Service();C(Uart_TakeErrors()&(UART_ERR_TX_TIMEOUT|UART_ERR_ABORT));
    C(Uart_TxBusy());memset(data,0x35,sizeof(data));C(!Uart_TxSubmit(data,256));C(tx[0]==0xA7);
    mock_tc=1;HAL_UART_TxCpltCallback(&huart1);C(!Uart_TxComplete());
    recover();C(Uart_TxBusy());recover();C(Uart_TxBusy());recover();C(!Uart_TxBusy());
    C(!Uart_TxComplete());C(Uart_TxSubmit(data,256));mock_tc=1;HAL_UART_TxCpltCallback(&huart1);C(Uart_TxComplete());
    /* RX中止未确认时不得清DR/重启；故障会话队列在中止重试期间一直不可读。 */
    (void)Uart_TakeErrors();feed(10,42);fail_abort_rx=3;mock_rxne=1;n=rx_starts;
    huart1.ErrorCode=HAL_UART_ERROR_FE;HAL_UART_ErrorCallback(&huart1);
    C((Uart_TakeErrors()&(UART_ERR_FE|UART_ERR_ABORT))==(UART_ERR_FE|UART_ERR_ABORT));
    C(!Uart_ReadByte(&b));recover();C(rx_starts==n && mock_rxne==1);
    recover();C(rx_starts==n && mock_rxne==1);recover();C(rx_starts==n+1 && mock_rxne==0);
    (void)Uart_TakeErrors();feed(1,61);C(Uart_ReadByte(&b)&&b==61);
    /* Fence失败同样隔离，不能在一次调用内立即再次Abort以吞掉失败。 */
    fail_abort_rx=2;mock_rxne=1;n=rx_starts;C(!Uart_RxFence());C(fail_abort_rx==1);
    C(Uart_TakeErrors()&UART_ERR_ABORT);recover();C(rx_starts==n && mock_rxne==1);
    recover();C(rx_starts==n+1 && mock_rxne==0);
    /* DMA TE分支必须报告TX中止失败，而非忽略HAL返回值。 */
    (void)Uart_TakeErrors();C(Uart_TxSubmit(data,1));fail_abort_tx=1;
    huart1.ErrorCode=HAL_UART_ERROR_DMA;HAL_UART_ErrorCallback(&huart1);
    C((Uart_TakeErrors()&(UART_ERR_DMA|UART_ERR_ABORT))==(UART_ERR_DMA|UART_ERR_ABORT));
    C(Uart_TxBusy());recover();C(!Uart_TxBusy() && !Uart_TxComplete());
#ifdef IAP_STORAGE_ENABLED
    (void)Uart_TakeErrors();C(Uart_TxSubmit(data,1));C(!Uart_PauseReceive());
    mock_tc=1;HAL_UART_TxCpltCallback(&huart1);C(Uart_TxComplete());
    feed(4,33);C(Uart_PauseReceive());n=rx_starts;recover();recover();C(rx_starts==n);C(!Uart_ReadByte(&b));
    C(Uart_ResumeReceive());C(rx_starts==n+1);feed(1,19);C(Uart_ReadByte(&b)&&b==19);
    fail_abort_rx=1;n=rx_starts;C(!Uart_PauseReceive());C(Uart_TakeErrors()&UART_ERR_ABORT);
    recover();C(rx_starts==n+1); /* 失败不允许永久锁在paused，正常错误恢复仍应前进。 */
    C(Uart_PauseReceive());fail_rx=1;n=rx_starts;C(!Uart_ResumeReceive());
    C(Uart_TakeErrors()&UART_ERR_RX_START);recover();C(rx_starts==n+2);
#endif
    printf("{\"assertions\":%u,\"random_trials\":10000,\"result\":\"PASS\"}\n",checks);return 0;
}

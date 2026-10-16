/* F1寄存器副作用夹具：不是HAL stub。CPAR/CMAR是真实32位HOST地址。
 * 硬件DMA按CNDTR循环写内存；IFCR W1C、SR/DR读取清RX标志、EN故障独立注入。 */
#include "ll_device_model.h"
#include "uart_port.h"
#include "uart_ll.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
USART_TypeDef model_uart;
DMA_Channel_TypeDef model_rx,model_tx;
DMA_TypeDef model_dma;
uint32_t mock_primask,mock_ipsr,mock_basepri,mock_faultmask,mock_cycles;
static uint32_t now,cycle_step,seed=1234567;
static unsigned checks,fail_rx,fail_tx,fail_abort_rx,fail_abort_tx,clear_rx_count;
#define C(x) do {checks++;if(!(x)){fprintf(stderr,"line %d %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t HAL_GetTick(void) {return now;}
uint32_t device_cycles(void) {mock_cycles+=cycle_step;return mock_cycles;}
uint32_t device_sr(void) {return model_uart.SR;}
void device_clear_rx(void)
{
    volatile uint32_t sr=model_uart.SR,dr=model_uart.DR;
    (void)sr;(void)dr;clear_rx_count++;
    model_uart.SR &= ~(USART_SR_IDLE|USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE|USART_SR_RXNE);
}
void device_clear_tc(void) {model_uart.SR &= ~USART_SR_TC;}
void device_clear_dma(uint32_t flags)
{
    model_dma.IFCR=flags;
    if(flags&DMA_IFCR_CGIF4) {model_dma.ISR &= ~(15U<<12);}
    if(flags&DMA_IFCR_CGIF5) {model_dma.ISR &= ~(15U<<16);}
}
void device_disable(DMA_Channel_TypeDef *channel)
{
    unsigned *fail=channel==&model_rx ? &fail_abort_rx : &fail_abort_tx;
    if(*fail) {if(*fail!=0xFFFFFFFFU){(*fail)--;}return;}
    channel->CCR &= ~DMA_CCR_EN;
}
void device_enable(DMA_Channel_TypeDef *channel)
{
    unsigned *fail=channel==&model_rx ? &fail_rx : &fail_tx;
    if(*fail) {(*fail)--;return;}
    C(!(channel->CCR&DMA_CCR_EN));C(channel->CPAR==(uint32_t)&model_uart.DR);
    channel->CCR |= DMA_CCR_EN;
}
static void rx_irq(void) {mock_ipsr=32;Uart_LL_RX_IRQHandler();mock_ipsr=0;}
static void tx_irq(void) {mock_ipsr=31;Uart_LL_TX_IRQHandler();mock_ipsr=0;}
static void idle_irq(void) {mock_ipsr=37;model_uart.SR|=USART_SR_IDLE;Uart_LL_USART_IRQHandler();mock_ipsr=0;}
static void feed(unsigned n,uint8_t start,int service)
{
    unsigned i,pos;uint8_t *rx=(uint8_t *)model_rx.CMAR;
    C(model_uart.CR3&USART_CR3_DMAR);C(model_rx.CCR&DMA_CCR_EN);
    for(i=0;i<n;i++) {
        pos=512U-model_rx.CNDTR;rx[pos]=(uint8_t)(start+i);model_rx.CNDTR--;
        if(model_rx.CNDTR==256U) {model_dma.ISR|=DMA_ISR_HTIF5;if(service) {rx_irq();}}
        if(model_rx.CNDTR==0U) {model_dma.ISR|=DMA_ISR_TCIF5;model_rx.CNDTR=512U;if(service) {rx_irq();}}
    }
    if(service) {idle_irq();}
}
static void recover(void) {now+=5;mock_cycles+=360000;Uart_Service();}
static void tx_done(void) {model_dma.ISR|=DMA_ISR_TCIF4;tx_irq();model_uart.SR|=USART_SR_TC;Uart_LL_USART_IRQHandler();}
static unsigned rnd(void) {seed=seed*1664525U+1013904223U;return seed;}
int main(void)
{
    unsigned i,j,n,clears;uint8_t byte,data[256];UartStats s;
    uint32_t trace_digest=2166136261U,trace_bytes=0U;
    C(sizeof(void*)==4);model_uart.SR=USART_SR_TC;
    model_uart.BRR=625;model_uart.CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE;
    C(Uart_LL_Quiescent());model_uart.BRR=1;C(!Uart_LL_Quiescent());model_uart.BRR=625;
    fail_rx=1;C(!Uart_Start());C(Uart_TakeErrors()==UART_ERR_RX_START);recover();
    C((model_rx.CCR&(DMA_CCR_CIRC|DMA_CCR_MINC|DMA_CCR_HTIE|DMA_CCR_TCIE|DMA_CCR_TEIE))==(DMA_CCR_CIRC|DMA_CCR_MINC|DMA_CCR_HTIE|DMA_CCR_TCIE|DMA_CCR_TEIE));
    for(i=0;i<10000;i++) {
        n=rnd()%257U;Uart_Service();feed(n,(uint8_t)i,1);idle_irq();rx_irq();
        for(j=0;j<n;j++) {C(Uart_ReadByte(&byte));C(byte==(uint8_t)(i+j));trace_digest=(trace_digest^byte)*16777619U;trace_bytes++;}
        C(!Uart_ReadByte(&byte));now++;mock_cycles+=72000;
    }
    /* HT/TC重叠与CNDTR同位置整圈：必须废弃，不凭delta==0当空。 */
    C(Uart_RxFence());feed(512,0,0);idle_irq();C(Uart_TakeErrors()&UART_ERR_RX_OVERRUN);recover();
    feed(512,0,0);rx_irq();C(Uart_TakeErrors()&UART_ERR_RX_OVERRUN);recover();
    for(i=0;i<8;i++) {feed(256,0,1);}C(Uart_TakeErrors()&UART_ERR_OVERFLOW);C(!Uart_ReadByte(&byte));recover();
    mock_cycles+=1440000;idle_irq();C(Uart_TakeErrors()&UART_ERR_RX_OVERRUN);recover();
    mock_cycles=0xFFFFFF00U;Uart_Service();(void)Uart_TakeErrors();recover();
    feed(1,77,1);C(Uart_ReadByte(&byte)&&byte==77);
    /* IDLE清除也清所有线路错误；同时四错的SR快照必须完整保留。 */
    model_uart.SR|=USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE|USART_SR_RXNE;
    idle_irq();C(Uart_TakeErrors()==(UART_ERR_ORE|UART_ERR_FE|UART_ERR_NE|UART_ERR_PE|UART_ERR_RX_OVERRUN));
    C(!(model_uart.SR&63U));recover();
    model_dma.ISR|=DMA_ISR_TEIF5|DMA_ISR_TCIF5;rx_irq();C(Uart_TakeErrors()==UART_ERR_DMA);recover();
    memset(data,0xA7,sizeof(data));model_uart.SR|=USART_SR_TC;C(Uart_TxSubmit(data,256));
    C(!(model_uart.SR&USART_SR_TC));C(!(model_tx.CCR&DMA_CCR_CIRC));memset(data,0,sizeof(data));C(((uint8_t*)model_tx.CMAR)[0]==0xA7);
    model_uart.SR|=USART_SR_TC;C(!Uart_TxComplete());model_uart.SR&=~USART_SR_TC;
    model_dma.ISR|=DMA_ISR_TCIF4;tx_irq();C(!Uart_TxComplete());model_uart.SR|=USART_SR_TC;C(Uart_TxComplete());
    C(Uart_TxSubmit(data,1));model_dma.ISR|=DMA_ISR_TEIF4|DMA_ISR_TCIF4;tx_irq();
    model_uart.SR|=USART_SR_TC;C(!Uart_TxComplete());C(Uart_TakeErrors()&UART_ERR_DMA);
    model_dma.ISR|=DMA_ISR_TCIF4;tx_irq();C(!Uart_TxComplete());
    C(!(model_uart.CR3&USART_CR3_DMAR));recover();
    C(Uart_TxSubmit(data,1));model_dma.ISR|=DMA_ISR_TEIF5|DMA_ISR_TCIF5;rx_irq();
    model_uart.SR|=USART_SR_TC;C(!Uart_TxComplete());C(Uart_TakeErrors()&UART_ERR_DMA);recover();
    C(Uart_TxSubmit(data,1));fail_abort_tx=3;now+=100;Uart_Service();C(Uart_TakeErrors()&(UART_ERR_ABORT|UART_ERR_TX_TIMEOUT));
    C(Uart_TxBusy());C(!Uart_TxSubmit(data,1));model_dma.ISR|=DMA_ISR_TCIF4;tx_irq();model_uart.SR|=USART_SR_TC;C(!Uart_TxComplete());
    recover();C(Uart_TxBusy());recover();C(Uart_TxBusy());recover();C(!Uart_TxBusy());
    C(Uart_TxSubmit(data,1));tx_done();C(Uart_TxComplete());
    feed(10,8,1);clears=clear_rx_count;fail_abort_rx=3;model_uart.SR|=USART_SR_FE|USART_SR_RXNE;idle_irq();
    C(Uart_TakeErrors()==(UART_ERR_FE|UART_ERR_ABORT|UART_ERR_RX_OVERRUN));C(clear_rx_count==clears);C(!Uart_ReadByte(&byte));
    recover();C(clear_rx_count==clears);recover();C(clear_rx_count==clears);recover();C(clear_rx_count==clears+1);
    (void)Uart_TakeErrors();feed(1,61,1);C(Uart_ReadByte(&byte)&&byte==61);
    fail_abort_rx=2;C(!Uart_RxFence());C(Uart_TakeErrors()&UART_ERR_ABORT);recover();C(!(model_uart.CR3&USART_CR3_DMAR));recover();C(model_uart.CR3&USART_CR3_DMAR);
    C(Uart_PauseReceive());C(Uart_LL_Quiescent());recover();C(!(model_rx.CCR&DMA_CCR_EN));
    C(Uart_ResumeReceive());C(!Uart_LL_Quiescent());
    fail_abort_rx=1;C(!Uart_PauseReceive());C(Uart_TakeErrors()&UART_ERR_ABORT);recover();C(model_uart.CR3&USART_CR3_DMAR);
    C(Uart_PauseReceive());fail_rx=1;C(!Uart_ResumeReceive());C(Uart_TakeErrors()&UART_ERR_RX_START);recover();C(model_uart.CR3&USART_CR3_DMAR);
    /* 持续恢复失败：每5ms一次，停Tick不忙重试；DMA缓冲不提前释放。 */
    fail_abort_rx=0xFFFFFFFFU;model_uart.SR|=USART_SR_ORE;idle_irq();n=fail_abort_rx;
    for(i=0;i<50;i++) {Uart_Service();}C(fail_abort_rx==n);C(!(model_uart.CR3&USART_CR3_DMAR));
    for(i=0;i<10;i++) {recover();}C(fail_abort_rx==n);fail_abort_rx=0;recover();(void)Uart_TakeErrors();
    fail_tx=1;C(!Uart_TxSubmit(data,1));C(Uart_TakeErrors()&UART_ERR_TX_START);
    memset(data,0x5A,sizeof(data));C(Uart_TxSubmit(data,256));fail_abort_tx=0xFFFFFFFFU;
    now+=100;Uart_Service();C((Uart_TakeErrors()&(UART_ERR_TX_TIMEOUT|UART_ERR_ABORT))==(UART_ERR_TX_TIMEOUT|UART_ERR_ABORT));
    memset(data,0xC3,sizeof(data));
    for(i=0;i<10;i++) {recover();C(Uart_TxBusy());C(!Uart_TxSubmit(data,256));C(((uint8_t*)model_tx.CMAR)[0]==0x5A);}
    model_dma.ISR|=DMA_ISR_TCIF4;tx_irq();model_uart.SR|=USART_SR_TC;C(!Uart_TxComplete());
    fail_abort_tx=0;recover();C(!Uart_TxBusy()&&!Uart_TxComplete());(void)Uart_TakeErrors();
    C(Uart_TxSubmit(data,1));tx_irq();C(!Uart_TxComplete());tx_done();C(Uart_TxComplete());
    now=0xFFFFFFFAU;C(Uart_TxSubmit(data,1));now+=100U;Uart_Service();C(Uart_TakeErrors()&UART_ERR_TX_TIMEOUT);
    C(Uart_TxSubmit(data,1));cycle_step=72000;C(!Uart_FlushTC(2));C(Uart_TakeErrors()&UART_ERR_TX_TIMEOUT);cycle_step=0;
    /* Tick和DWT同时停住时最后由有限轮询预算退出。 */
    C(Uart_TxSubmit(data,1));C(!Uart_FlushTC(1));C(Uart_TakeErrors()&UART_ERR_TX_TIMEOUT);
    C(Uart_TxSubmit(data,1));tx_done();C(Uart_TxComplete());
    Uart_GetStats(&s);C(s.queued_bytes==0&&s.recoveries>0);
    printf("{\"result\":\"PASS\",\"assertions\":%u,\"random_trials\":10000,\"sr_dr_clears\":%u,\"trace_digest\":%lu,\"trace_bytes\":%lu}\n",checks,clear_rx_count,(unsigned long)trace_digest,(unsigned long)trace_bytes);
    return 0;
}

/* 专用USART1+DMA4/5后端，已由Phase7/8 Source Group选择；无HAL UART句柄/回调。
 * 512B Circular RX、2048B Ring、256B静态TX；三IRQ同优先级6。
 * 与DESIGN_SPEC §19.2 HAL IRQ契约有差异，按用户2026-10-14授权验证，未改规格。
 * 不复制第三方驱动；ST LL/CMSIS来自本工程只读Drivers，许可随厂商文件保留。
 */
#include "uart_port.h"
#include "ring_buffer.h"
#include "dma_cursor.h"
#include "uart_ll.h"
#include "uart_ll_device.h"
#include <string.h>
#include "control_zi.h"
#define UART_CONTROL BOOT_CONTROL_ZI
static RingBuffer rx_ring;
static uint8_t rx_dma[512];
static UART_CONTROL uint16_t rx_last;
static UART_CONTROL uint32_t watch_cycle;
#define RX_GUARD_CYCLES (72000U * 20U)
static uint8_t tx_buffer[UART_TX_MAX];
static volatile UartStats stats;
static UART_CONTROL volatile uint8_t running, rx_active, tx_active, tx_callback, tx_failed;
#ifdef IAP_STORAGE_ENABLED
static UART_CONTROL uint8_t receive_paused;
#endif
static UART_CONTROL volatile uint8_t rx_abort_pending, tx_abort_pending;
static UART_CONTROL uint16_t tx_length;
static UART_CONTROL uint32_t tx_start, tx_cycle, rx_retry, tx_retry;
static void stop_tx(uint32_t bits);
/* 固定两个通道共用机械停机步骤，掩码由本owner的窄包装给定。
 * EN清零仅一次读回，不在ISR忙等；永久EN保持缓冲占用及5ms恢复。 */
static __attribute__((noinline)) int abort_dma(DMA_Channel_TypeDef *channel,
    uint32_t request,uint32_t interrupts,uint32_t flags)
{
    USART1->CR3 &= ~request;
    USART1->CR1 &= ~interrupts;
    device_disable(channel);
    if(channel->CCR&DMA_CCR_EN) {return 0;}
    device_clear_dma(flags);return 1;
}
static int abort_rx(void)
{return abort_dma(DMA1_Channel5,USART_CR3_DMAR|USART_CR3_EIE,
    USART_CR1_IDLEIE|USART_CR1_PEIE,DMA_IFCR_CGIF5);}
static int abort_tx(void)
{return abort_dma(DMA1_Channel4,USART_CR3_DMAT,USART_CR1_TCIE,DMA_IFCR_CGIF4);}
static int start_rx(void)
{
    if(DMA1_Channel5->CCR&DMA_CCR_EN) {return 0;}
    device_clear_dma(DMA_IFCR_CGIF5);
    DMA1_Channel5->CCR=DMA_CCR_MINC|DMA_CCR_CIRC|DMA_CCR_PL_1|DMA_CCR_HTIE|DMA_CCR_TCIE|DMA_CCR_TEIE;
    DMA1_Channel5->CPAR=(uint32_t)&USART1->DR;
    DMA1_Channel5->CMAR=(uint32_t)rx_dma;
    DMA1_Channel5->CNDTR=512U;
    device_enable(DMA1_Channel5);
    if(!(DMA1_Channel5->CCR&DMA_CCR_EN)) {return 0;}
    USART1->CR1 |= USART_CR1_IDLEIE|USART_CR1_PEIE;
    USART1->CR3 |= USART_CR3_DMAR|USART_CR3_EIE;
    return 1;
}
static int start_tx(uint16_t length)
{
    if(DMA1_Channel4->CCR&DMA_CCR_EN) {return 0;}
    device_clear_dma(DMA_IFCR_CGIF4); device_clear_tc();
    DMA1_Channel4->CCR=DMA_CCR_MINC|DMA_CCR_DIR|DMA_CCR_PL_0|DMA_CCR_TCIE|DMA_CCR_TEIE;
    DMA1_Channel4->CPAR=(uint32_t)&USART1->DR;
    DMA1_Channel4->CMAR=(uint32_t)tx_buffer;
    DMA1_Channel4->CNDTR=length;
    device_enable(DMA1_Channel4);
    if(!(DMA1_Channel4->CCR&DMA_CCR_EN)) {return 0;}
    USART1->CR3 |= USART_CR3_DMAT;
    return 1;
}

static uint32_t lock_irq(void)
{
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    return key;
}
static void unlock_irq(uint32_t key) { __set_PRIMASK(key); }
static int foreground(void)
{
    return (__get_IPSR() == 0U) && (__get_PRIMASK() == 0U) &&
           (__get_BASEPRI() == 0U) && (__get_FAULTMASK() == 0U);
}
static void error(uint32_t bits)
{
    stats.pending_errors |= bits;
    stats.total_errors++;
}
/* 仅作废队列，不改变错误锁存、重试时间或DMA状态；调用者保持原顺序。 */
static __attribute__((noinline)) void discard_queue(void)
{
    stats.discarded_bytes+=Ring_Count(&rx_ring);Ring_Reset(&rx_ring);
}
static void stop_rx(uint32_t bits)
{
    /* 先停止接收，再作废整个队列；错误包绝不能跨恢复边界继续消费。 */
    error(bits);
    rx_active = 0U;
    rx_abort_pending = (!abort_rx());
    if (rx_abort_pending) { error(UART_ERR_ABORT); }
    discard_queue();
    rx_retry = HAL_GetTick();
}
/* 与原HAL DMA错误契约一致：任一方向TE均废弃整条双向会话。 */
static void dma_fault(void)
{
    if(tx_active){stop_tx(UART_ERR_DMA);}
    if(rx_active){stop_rx(UART_ERR_DMA);}
}
static int arm_rx(void)
{
    rx_last = 0U;
    watch_cycle = device_cycles();
    if (!start_rx()) {
        stop_rx(UART_ERR_RX_START);
        return 0;
    }
    rx_active = 1U;
    return 1;
}
static void flush_stopped_rx(void)
{
    /* 仅在DMA停止已确认后由本owner执行SR/DR清除。 */
    if (device_sr() & USART_SR_RXNE) {
        stats.discarded_bytes++;
    }
    device_clear_rx();
}
int Uart_Start(void)
{
    uint32_t key;
    int ok;
    if (!foreground() || running) { return 0; }
    key = lock_irq();
    Ring_Reset(&rx_ring);
    memset((void *)&stats, 0, sizeof(stats));
    tx_active = tx_callback = tx_failed = 0U;
    rx_abort_pending = tx_abort_pending = 0U;
    #ifndef UART_LL_HOST_TEST
    /* 启用M3周期计数，用于HAL tick被屏蔽时的覆盖守卫。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) || SystemCoreClock != 72000000U) {
        unlock_irq(key); return 0;
    }
#endif
    running = 1U;
    flush_stopped_rx();
    ok = arm_rx();
    unlock_irq(key);
    return ok;
}
static void collect_rx(void)
{
    uint16_t start, count;uint32_t i;
    if (!running || !rx_active) { return; }
    if ((uint32_t)(device_cycles()-watch_cycle)>=RX_GUARD_CYCLES ||
        (DMA1->ISR & (DMA_ISR_HTIF5|DMA_ISR_TCIF5)) == (DMA_ISR_HTIF5|DMA_ISR_TCIF5)) {
        stop_rx(UART_ERR_RX_OVERRUN); return;
    }
    start=rx_last;
    if (!DmaCursor_Advance(&rx_last,(uint16_t)DMA1_Channel5->CNDTR,&count)) {
        stop_rx(UART_ERR_RX_OVERRUN); return;
    }
    __DMB();
    for(i=0U;i<count;++i) {
        stats.received_bytes++;
        if(!Ring_Push(&rx_ring,rx_dma[(start+i)&511U])) {
            stats.discarded_bytes+=count-i; stop_rx(UART_ERR_OVERFLOW); return;
        }
    }
}
/* 唯一SR快照；IDLE与线路错误同时存在时先锁错，禁止清IDLE吞掉错误现场。 */
void Uart_LL_USART_IRQHandler(void)
{
    uint32_t sr=device_sr(),bits=0U;
    if(!running) {return;}
    if(sr&USART_SR_ORE) {bits|=UART_ERR_ORE;}
    if(sr&USART_SR_FE) {bits|=UART_ERR_FE;}
    if(sr&USART_SR_NE) {bits|=UART_ERR_NE;}
    if(sr&USART_SR_PE) {bits|=UART_ERR_PE;}
    /* IDLE时DR尚未被DMA取走：CPU清IDLE会消费该字节，必须废弃会话，不能静默丢字节。 */
    if((sr&(USART_SR_IDLE|USART_SR_RXNE))==(USART_SR_IDLE|USART_SR_RXNE)) {bits|=UART_ERR_RX_OVERRUN;}
    if(bits && rx_active) {
        stop_rx(bits);
        if(!rx_abort_pending) {flush_stopped_rx();}
    } else if((sr&USART_SR_IDLE) && rx_active) {
        /* SR已快照，DR读取按F1清除序列；DMA与CPU清标志竞争仍需实物验收。 */
        device_clear_rx(); collect_rx();
    }
    if((sr&USART_SR_TC) && (USART1->CR1&USART_CR1_TCIE)) {
        USART1->CR1 &= ~USART_CR1_TCIE;
        /* DMA完成是独立条件；迟到IRQ无法使已失败代际成功。 */
    }
}
void Uart_LL_RX_IRQHandler(void)
{
    uint32_t flags=DMA1->ISR;
    /* USART与DMA同优先级：DMA先被调度也不得发布已带线路错误的字节。 */
    if(device_sr()&(USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
        Uart_LL_USART_IRQHandler();
    }
    if(rx_active && (flags&DMA_ISR_TEIF5)) {dma_fault();}
    else if(rx_active && (flags&(DMA_ISR_HTIF5|DMA_ISR_TCIF5))) {collect_rx();}
    device_clear_dma(DMA_IFCR_CGIF5);
}
void Uart_LL_TX_IRQHandler(void)
{
    uint32_t flags=DMA1->ISR;
    if(tx_active && (flags&DMA_ISR_TEIF4)) {dma_fault();}
    else if(tx_active && (flags&DMA_ISR_TCIF4)) {
        /* 先关闭DMA请求并确认EN=0，然后公布DMA完成。TE优先于TC。 */
        if(!abort_tx()) {stop_tx(UART_ERR_ABORT);}
        else {tx_callback=1U;USART1->CR1|=USART_CR1_TCIE;}
    }
    device_clear_dma(DMA_IFCR_CGIF4);
}
static void stop_tx(uint32_t bits)
{
    error(bits);
    tx_active = 0U;
    tx_callback = 0U;
    tx_failed = 1U;
    /* 硬件中止失败时仍可能引用tx_buffer。保留所有权，迟到callback/TC不能
       报成功；每5ms仅重试一次硬件中止，确认成功后才允许下一次提交。 */
    tx_abort_pending = (!abort_tx());
    if (tx_abort_pending) { error(UART_ERR_ABORT); }
    tx_retry = HAL_GetTick();
}
void Uart_Service(void)
{
    uint32_t now, key;
    if (!running || !foreground()) { return; }
    key = lock_irq();
    /* 与ISR写入的rx_retry同一临界区取时刻；否则IRQ恰好更新rx_retry时，
       旧now减新起点可能无符号回绕，错误地跳过5ms恢复等待。 */
    now = HAL_GetTick();
    if (rx_active && (uint32_t)(device_cycles() - watch_cycle) >= RX_GUARD_CYCLES) {
        stop_rx(UART_ERR_RX_OVERRUN);
    }
    watch_cycle = device_cycles();
    if (!rx_active &&
#ifdef IAP_STORAGE_ENABLED
        !receive_paused &&
#endif
        (uint32_t)(now - rx_retry) >= 5U) {
        int pending=rx_abort_pending;
        /* 未确认停收，禁止SR/DR清除与DMA重新启动；每次服务至多一次重试，
           持续失败保持隔离并记录错误，不使用忙等或假恢复。 */
        /* IRQ保持屏蔽；下列调用不会重入UART或改变pending。
         * 仍在原时点写回共享标志，只在本临界区使用刚取得的返回值。 */
        if (pending) {
            pending=!abort_rx();rx_abort_pending=(uint8_t)pending;
            if (pending) { error(UART_ERR_ABORT); rx_retry = now; }
        }
        if (!pending) {
            flush_stopped_rx();
            if (arm_rx()) { stats.recoveries++; }
        }
    }
    if (tx_abort_pending && (uint32_t)(now - tx_retry) >= 5U) {
        int pending=!abort_tx();tx_abort_pending=(uint8_t)pending;
        if (pending) { error(UART_ERR_ABORT); }
        tx_retry = now;
    }
    if (tx_active) {
        if (tx_callback && (device_sr() & USART_SR_TC)) {
            USART1->CR1 &= ~USART_CR1_TCIE;
            tx_active = 0U;
            stats.transmitted_bytes += tx_length;
        } else if ((uint32_t)(now - tx_start) >= UART_TX_TIMEOUT_MS ||
                   (uint32_t)(device_cycles()-tx_cycle)>=72000U*UART_TX_TIMEOUT_MS) {
            stop_tx(UART_ERR_TX_TIMEOUT);
        }
    }
    unlock_irq(key);
}
int Uart_ReadByte(uint8_t *byte)
{
    uint32_t key;
    int ok;
    if (!foreground() || (byte == 0) || !running) { return 0; }
    key = lock_irq();
    ok = (stats.pending_errors == 0U) ? Ring_Pop(&rx_ring, byte) : 0;
    unlock_irq(key);
    return ok;
}
int Uart_RxFence(void)
{
    uint32_t key;int ok;
    if (!running || !foreground()) {return 0;}
    key=lock_irq();rx_active=0U;
    rx_abort_pending=!abort_rx();
    discard_queue();
    if(rx_abort_pending) {
        error(UART_ERR_ABORT);rx_retry=HAL_GetTick();unlock_irq(key);return 0;
    }
    flush_stopped_rx();ok=arm_rx();unlock_irq(key);return ok;
}
#ifdef IAP_STORAGE_ENABLED
int Uart_PauseReceive(void)
{
    uint32_t key;
    if(!running || !foreground() || !Uart_TxComplete()){return 0;}
    key=lock_irq();receive_paused=1U;rx_active=0U;
    rx_abort_pending=!abort_rx();
    discard_queue();
    if(rx_abort_pending){
        error(UART_ERR_ABORT);receive_paused=0U;rx_retry=HAL_GetTick();
    }
    unlock_irq(key);return !rx_abort_pending;
}
int Uart_ResumeReceive(void)
{
    int ok;
    if(!running || !foreground() || !receive_paused){return 0;}
    ok=Uart_RxFence();
    /* 恢复失败已由RxFence关闭RX并报告；交回有界错误恢复，不能永久paused。 */
    receive_paused=0U;
    return ok;
}
#endif
uint32_t Uart_TakeErrors(void)
{
    uint32_t key, bits;
    if (!foreground()) { return 0U; }
    key = lock_irq();
    bits = stats.pending_errors;
    stats.pending_errors = 0U;
    unlock_irq(key);
    return bits;
}
void Uart_GetStats(UartStats *out)
{
    uint32_t key;
    if ((out == 0) || !foreground()) { return; }
    key = lock_irq();
    *out = stats;
    out->queued_bytes = Ring_Count(&rx_ring);
    out->rx_ready = rx_active;
    out->tx_busy = tx_active || tx_abort_pending;
    unlock_irq(key);
}
int Uart_TxSubmit(const uint8_t *data, uint16_t length)
{
    uint32_t key;
    if (!running || !foreground() || !data || !length || length > UART_TX_MAX) { return 0; }
    Uart_Service();
    if (tx_active || tx_abort_pending) { return 0; }
    /* 前台唯一提交者；拷贝时不屏蔽RX中断，防止256B拷贝导致UART过载。 */
    memcpy(tx_buffer, data, length);
    key = lock_irq();
    tx_length = length;
    tx_start = HAL_GetTick();
    tx_cycle = device_cycles();
    tx_callback = tx_failed = 0U;
    tx_active = 1U;
    if (!start_tx(length)) {
        stop_tx(UART_ERR_TX_START);
        unlock_irq(key);
        return 0;
    }
    unlock_irq(key);
    return 1;
}
int Uart_TxComplete(void)
{
    Uart_Service();
    return running && !tx_active && !tx_abort_pending && !tx_failed;
}
int Uart_TxBusy(void)
{
    Uart_Service();return tx_active || tx_abort_pending;
}
int Uart_FlushTC(uint32_t timeout_ms)
{
    uint32_t start, key, cycle, budget=3600000U;
    if (!foreground() || !running || timeout_ms == 0U || timeout_ms > 1000U) { return 0; }
    start = HAL_GetTick(); cycle=device_cycles();
    while (!Uart_TxComplete()) {
        if (tx_failed) { return 0; }
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms ||
            (uint32_t)(device_cycles()-cycle)>=72000U*timeout_ms || !--budget) {
            key = lock_irq();
            if (tx_active) { stop_tx(UART_ERR_TX_TIMEOUT); }
            unlock_irq(key);
            return 0;
        }
    }
    return 1;
}

int Uart_LL_Quiescent(void)
{
    /* 正常冷启可在Uart_Start之前直接跳已确认App；此时仍核验硬件初始化与停请求。
     * 会话启动后则必须由Pause明确停收，不能凭未活跃状态绕过恢复失败。 */
    return (!running || receive_paused) && USART1->BRR==625U &&
           (USART1->CR1&(USART_CR1_UE|USART_CR1_TE|USART_CR1_RE))==
             (USART_CR1_UE|USART_CR1_TE|USART_CR1_RE) && !rx_active && !rx_abort_pending &&
           !tx_active && !tx_abort_pending && !tx_failed && (device_sr()&USART_SR_TC) &&
           !(USART1->CR1&(USART_CR1_IDLEIE|USART_CR1_PEIE|USART_CR1_TCIE)) &&
           !(USART1->CR3&(USART_CR3_DMAR|USART_CR3_DMAT)) &&
           !(DMA1_Channel4->CCR&DMA_CCR_EN) && !(DMA1_Channel5->CCR&DMA_CCR_EN);
}

/* USART1唯一owner。参考C04-B的SPSC/TC思想，HAL IT及错误恢复重新实现。 */
#include "uart_port.h"
#include "ring_buffer.h"
#include "usart.h"
#include <string.h>
static RingBuffer rx_ring;
static uint8_t rx_byte;
static uint8_t tx_buffer[UART_TX_MAX];
static volatile UartStats stats;
static volatile uint8_t running, rx_active, tx_active, tx_callback, tx_failed;
static uint16_t tx_length;
static uint32_t tx_start, rx_retry;
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
static void stop_rx(uint32_t bits)
{
    /* 先停止接收，再作废整个队列；错误包绝不能跨恢复边界继续消费。 */
    error(bits);
    if (HAL_UART_AbortReceive(&huart1) != HAL_OK) { error(UART_ERR_ABORT); }
    rx_active = 0U;
    stats.discarded_bytes += Ring_Count(&rx_ring);
    Ring_Reset(&rx_ring);
    rx_retry = HAL_GetTick();
}
static int arm_rx(void)
{
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) {
        stop_rx(UART_ERR_RX_START);
        return 0;
    }
    rx_active = 1U;
    return 1;
}
static void flush_stopped_rx(void)
{
    /* HAL AbortReceive只关闭IT/恢复句柄，不读取SR/DR。仅停收恢复时按HAL宏
       清残留ORE/FE/NE和DR；正常IRQ仍由HAL独占，无手写IDLE/IRQ清标志。 */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET) {
        stats.discarded_bytes++;
    }
    __HAL_UART_CLEAR_OREFLAG(&huart1);
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
    running = 1U;
    flush_stopped_rx();
    ok = arm_rx();
    unlock_irq(key);
    return ok;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if ((uart != &huart1) || !running || !rx_active) { return; }
    /* HAL可能先调用RxCplt再调用Error；禁止将FE/NE/ORE污染的字节入队/重挂。 */
    if (uart->ErrorCode != HAL_UART_ERROR_NONE) {
        stats.discarded_bytes++;
        return;
    }
    stats.received_bytes++;
    if (!Ring_Push(&rx_ring, rx_byte)) {
        stats.discarded_bytes++;
        stop_rx(UART_ERR_OVERFLOW);
        return;
    }
    rx_active = 0U;
    (void)arm_rx();
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    uint32_t bits = 0U;
    if ((uart != &huart1) || !running) { return; }
    if (uart->ErrorCode & HAL_UART_ERROR_ORE) { bits |= UART_ERR_ORE; }
    if (uart->ErrorCode & HAL_UART_ERROR_FE) { bits |= UART_ERR_FE; }
    if (uart->ErrorCode & HAL_UART_ERROR_NE) { bits |= UART_ERR_NE; }
    if (uart->ErrorCode & HAL_UART_ERROR_PE) { bits |= UART_ERR_PE; }
    if (bits == 0U) { bits = UART_ERR_RX_START; }
    stop_rx(bits);
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    if ((uart == &huart1) && running && tx_active) { tx_callback = 1U; }
}
static void stop_tx(uint32_t bits)
{
    error(bits);
    if (HAL_UART_AbortTransmit(&huart1) != HAL_OK) { error(UART_ERR_ABORT); }
    tx_active = 0U;
    tx_callback = 0U;
    tx_failed = 1U;
}
void Uart_Service(void)
{
    uint32_t now, key;
    if (!running || !foreground()) { return; }
    key = lock_irq();
    /* 与ISR写入的rx_retry同一临界区取时刻；否则IRQ恰好更新rx_retry时，
       旧now减新起点可能无符号回绕，错误地跳过5ms恢复等待。 */
    now = HAL_GetTick();
    if (!rx_active && (uint32_t)(now - rx_retry) >= 5U) {
        flush_stopped_rx();
        if (arm_rx()) { stats.recoveries++; }
    }
    if (tx_active) {
        if (tx_callback && (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) != RESET)) {
            tx_active = 0U;
            stats.transmitted_bytes += tx_length;
        } else if ((uint32_t)(now - tx_start) >= UART_TX_TIMEOUT_MS) {
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
    out->tx_busy = tx_active;
    unlock_irq(key);
}
int Uart_TxSubmit(const uint8_t *data, uint16_t length)
{
    uint32_t key;
    if (!running || !foreground() || !data || !length || length > UART_TX_MAX) { return 0; }
    Uart_Service();
    if (tx_active) { return 0; }
    /* 前台唯一提交者；拷贝时不屏蔽RX中断，防止256B拷贝导致UART过载。 */
    memcpy(tx_buffer, data, length);
    key = lock_irq();
    tx_length = length;
    tx_start = HAL_GetTick();
    tx_callback = tx_failed = 0U;
    tx_active = 1U;
    if (HAL_UART_Transmit_IT(&huart1, tx_buffer, length) != HAL_OK) {
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
    return running && !tx_active && !tx_failed;
}
int Uart_FlushTC(uint32_t timeout_ms)
{
    uint32_t start, key;
    if (!foreground() || !running || timeout_ms == 0U || timeout_ms > 1000U) { return 0; }
    start = HAL_GetTick();
    while (!Uart_TxComplete()) {
        if (tx_failed) { return 0; }
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms) {
            key = lock_irq();
            if (tx_active) { stop_tx(UART_ERR_TX_TIMEOUT); }
            unlock_irq(key);
            return 0;
        }
    }
    return 1;
}

#ifndef UART_PORT_H
#define UART_PORT_H
#include <stdint.h>
#define UART_TX_MAX 256U
#define UART_TX_TIMEOUT_MS 100U
enum {
    UART_ERR_OVERFLOW = 1U,
    UART_ERR_ORE = 2U,
    UART_ERR_FE = 4U,
    UART_ERR_NE = 8U,
    UART_ERR_PE = 16U,
    UART_ERR_RX_START = 32U,
    UART_ERR_TX_START = 64U,
    UART_ERR_TX_TIMEOUT = 128U,
    UART_ERR_CONTEXT = 256U,
    UART_ERR_ABORT = 512U,
    UART_ERR_DMA = 1024U,
    UART_ERR_RX_OVERRUN = 2048U
};
typedef struct {
    uint32_t pending_errors;
    uint32_t total_errors;
    uint32_t discarded_bytes; /* 已观察并作废的字节下界；停收期间线上丢失数无法测定。 */
    uint32_t recoveries;
    uint32_t received_bytes;
    uint32_t transmitted_bytes;
    uint16_t queued_bytes;
    uint8_t rx_ready;
    uint8_t tx_busy;
} UartStats;
/** 简介：唯一USART1 owner。MX_USART1_UART_Init之后前台调用一次。
 * Source Group择一：Phase1 uart_port.c普通IT；其余HAL基线uart_dma.c；
 * Phase7/8产品选uart_ll.c专用LL/CMSIS，不能在同一外设叠加运行owner。
 * 返回1挂接成功；0失败，错误锁存且Service按5ms间隔自动重试。
 * DMA模式512B循环，前台每20ms内Service，保持HT开启、三IRQ同优先级6。
 */
int Uart_Start(void);
/* 前台非阻塞服务：恢复RX、TX硬件TC收尾及100ms超时abort。必须持续调用。 */
void Uart_Service(void);
/* 唯一前台消费者，返回1有效字节；0空/未读错误事件。错误后丢弃整个旧队列。 */
int Uart_ReadByte(uint8_t *byte);
/* DMA模式前台切换屏障：后端确认停收→清Ring/残留→重启DMA，废弃旧模式的所有字节。
 * 返回1新代接收就绪；0锁存故障，调用者不得继续旧协议会话。 */
int Uart_RxFence(void);
#ifdef IAP_STORAGE_ENABLED
/* 停等介质操作窗口：暂停RX并作废队列，Resume重新建立DMA代际。
 * 暂停时对端不得发送；不把此窗口当作连续双向流能力。 */
int Uart_PauseReceive(void);
int Uart_ResumeReceive(void);
#endif
/* 原子取走错误事件；协议调用者必须丢弃当前包/会话，不能当作成功接收。 */
uint32_t Uart_TakeErrors(void);
/* 一致快照；前台调用，统计uint32自然回绕，不能用它判定镜像可信。 */
void Uart_GetStats(UartStats *stats);
/** 简介：非阻塞发送（按工程选择IT/DMA），拷贝1..256B到owner静态缓冲。
 * 参数：data只需在调用期间有效；返回1接受，0忙/非法/上下文错误/启动失败。
 * 缓冲在后端DMA完成确认且USART硬件TC=1前不复用；DMA超时后须abort成功才允许重试，
 * 失败绝不代表线上字节发送完整，禁止据此BKP写入或复位。
 */
int Uart_TxSubmit(const uint8_t *data, uint16_t length);
/* 返回1仅表示最后一次已接受发送成功到TC（启动时也为1）；超时/失败为0。 */
int Uart_TxComplete(void);
/* DMA前台调度门：1尚占用缓冲或中止尚未成功；0可尝试新提交。
 * Busy=0不代表TC成功，复位/跳转仍只能使用TxComplete/FlushTC。 */
int Uart_TxBusy(void);
/** 前台有界等待最终TC，timeout为1..1000ms；必须IRQ开启、HAL Tick运行。
 * 返回1成功；0非法/超时/发送失败。超时中止，不允许继续执行复位/跳转。
 */
int Uart_FlushTC(uint32_t timeout_ms);
#endif

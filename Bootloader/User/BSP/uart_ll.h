#ifndef UART_LL_H
#define UART_LL_H
/* 专用USART1/DMA4/5候选；只由独立Source Group选择，不与HAL UART运行owner并存。 */
void Uart_LL_USART_IRQHandler(void);
void Uart_LL_RX_IRQHandler(void);
void Uart_LL_TX_IRQHandler(void);
/* Jump只读守卫：暂停成功、TX释放且真实TC、请求和DMA均停止；不伪造HAL READY。 */
int Uart_LL_Quiescent(void);
#endif

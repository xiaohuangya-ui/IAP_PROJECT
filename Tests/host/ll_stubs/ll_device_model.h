#ifndef LL_DEVICE_MODEL_H
#define LL_DEVICE_MODEL_H
#include <stdint.h>
#include <stddef.h>
typedef struct {volatile uint32_t SR,DR,BRR,CR1,CR2,CR3;} USART_TypeDef;
typedef struct {volatile uint32_t CCR,CNDTR,CPAR,CMAR;} DMA_Channel_TypeDef;
typedef struct {volatile uint32_t ISR,IFCR;} DMA_TypeDef;
extern USART_TypeDef model_uart;
extern DMA_Channel_TypeDef model_rx,model_tx;
extern DMA_TypeDef model_dma;
#define USART1 (&model_uart)
#define DMA1_Channel4 (&model_tx)
#define DMA1_Channel5 (&model_rx)
#define DMA1 (&model_dma)
#define USART_SR_PE 1U
#define USART_SR_FE 2U
#define USART_SR_NE 4U
#define USART_SR_ORE 8U
#define USART_SR_IDLE 16U
#define USART_SR_RXNE 32U
#define USART_SR_TC 64U
#define USART_CR1_IDLEIE 16U
#define USART_CR1_TCIE 64U
#define USART_CR1_PEIE 256U
#define USART_CR1_UE 8192U
#define USART_CR1_TE 8U
#define USART_CR1_RE 4U
#define USART_CR3_EIE 1U
#define USART_CR3_DMAR 64U
#define USART_CR3_DMAT 128U
#define DMA_CCR_EN 1U
#define DMA_CCR_TCIE 2U
#define DMA_CCR_HTIE 4U
#define DMA_CCR_TEIE 8U
#define DMA_CCR_DIR 16U
#define DMA_CCR_CIRC 32U
#define DMA_CCR_MINC 128U
#define DMA_CCR_PL_0 4096U
#define DMA_CCR_PL_1 8192U
#define DMA_ISR_TCIF4 (1U<<13)
#define DMA_ISR_HTIF4 (1U<<14)
#define DMA_ISR_TEIF4 (1U<<15)
#define DMA_ISR_TCIF5 (1U<<17)
#define DMA_ISR_HTIF5 (1U<<18)
#define DMA_ISR_TEIF5 (1U<<19)
#define DMA_IFCR_CGIF4 (1U<<12)
#define DMA_IFCR_CGIF5 (1U<<16)
extern uint32_t mock_primask,mock_ipsr,mock_basepri,mock_faultmask,mock_cycles;
#define __get_PRIMASK() mock_primask
#define __get_IPSR() mock_ipsr
#define __get_BASEPRI() mock_basepri
#define __get_FAULTMASK() mock_faultmask
#define __disable_irq() (mock_primask=1U)
#define __set_PRIMASK(k) (mock_primask=(k))
#define __DMB() __sync_synchronize()
uint32_t HAL_GetTick(void);
uint32_t device_cycles(void);
uint32_t device_sr(void);
void device_clear_rx(void);
void device_clear_tc(void);
void device_clear_dma(uint32_t flags);
void device_disable(DMA_Channel_TypeDef *channel);
void device_enable(DMA_Channel_TypeDef *channel);
#endif

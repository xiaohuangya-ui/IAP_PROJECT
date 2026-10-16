#ifndef UART_LL_DEVICE_H
#define UART_LL_DEVICE_H
/* 私有硬件端口。模型必须实现SR/DR、IFCR与EN副作用，不能用HAL回调替身。 */
#ifdef UART_LL_HOST_TEST
#include "ll_device_model.h"
#else
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_usart.h"
#include "stm32f1xx_ll_dma.h"
static uint32_t device_cycles(void) { return DWT->CYCCNT; }
static uint32_t device_sr(void) { return USART1->SR; }
static void device_clear_rx(void) { LL_USART_ClearFlag_IDLE(USART1); }
static void device_clear_tc(void) { LL_USART_ClearFlag_TC(USART1); }
static void device_clear_dma(uint32_t flags) { DMA1->IFCR=flags; }
static void device_disable(DMA_Channel_TypeDef *channel) { channel->CCR &= ~DMA_CCR_EN; }
static void device_enable(DMA_Channel_TypeDef *channel) { channel->CCR |= DMA_CCR_EN; }
#endif
#endif

#ifndef BOOT_CLOCK_H
#define BOOT_CLOCK_H
/* 唯一产品启动时钟owner：仅复位HSI上下文调用；失败停驻，不运行外设。
 * HSE8MHz/PLLx9/HCLK72/APB1 36/APB2 72，HAL保留SysTick初始化。
 * 不访问BDCR、不清Reset flags、不启动IWDG。 */
int BootClock_Init(void);
void BootClock_Config(void);
#ifdef IAP_PRODUCT_ENABLED
#include "stm32f1xx.h"
/* 产品已固定HCLK72；PPRE2的0xx均DIV1，1xx均分频。检查实际寄存器，
 * 不能只凭初始化曾成功就缓存PCLK可信性。 */
static __inline int BootClock_Pclk2Ready(void)
{return SystemCoreClock==72000000U&&(RCC->CFGR&RCC_CFGR_PPRE2_2)==0U;}
#endif
#endif

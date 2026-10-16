/* USART1专用后端唯一初始化owner；Source Group替换生成usart.c，不修改.ioc/Drivers。
 * 保持PA9 AF推挽、PA10上拉、115200 8N1；DMA时钟/IRQ4/5仍由生成MX_DMA_Init。
 * 停驻仍由Uart owner/Jump负责，不注册HAL句柄或回调。 */
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_usart.h"
#include "stm32f1xx_ll_gpio.h"
#include "boot_clock.h"
#ifdef BOOT_FIXED_IRQ
#include "boot_irq.h"
#endif
void Error_Handler(void);
void MX_USART1_UART_Init(void)
{
    if(
#ifdef IAP_PRODUCT_ENABLED
       !BootClock_Pclk2Ready() ||
#else
       SystemCoreClock!=72000000U || HAL_RCC_GetPCLK2Freq()!=72000000U ||
#endif
       (DMA1_Channel4->CCR&DMA_CCR_EN) ||
       (DMA1_Channel5->CCR&DMA_CCR_EN)) {Error_Handler();return;}
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* 先选上拉再切PA10输入模式；只修改PA9/10，保持SPI/SWD及其他引脚配置。
     * F1 CRH：PA9=0xB（50MHz AF推挽）、PA10=0x8（上/下拉输入）。 */
    LL_GPIO_SetOutputPin(GPIOA,LL_GPIO_PIN_10);
    MODIFY_REG(GPIOA->CRH,0xFF0U,0x8B0U);
    USART1->CR1=0U;USART1->CR2=0U;USART1->CR3=0U;
    /* 固定PCLK2=72MHz、16倍过采样：USARTDIV=39+1/16，BRR=0x271=625。
     * AC5将LL通用波特率的volatile除法重复展开；固定寄存器值保留同一时序。
     * 入口同时核验实际PCLK2，不能把HCLK72误当USART1时钟。 */
    LL_USART_WriteReg(USART1,BRR,625U);
    USART1->CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE;
#ifdef BOOT_FIXED_IRQ
    BootIrq_EnableExternal6(USART1_IRQn);
#else
    HAL_NVIC_SetPriority(USART1_IRQn,6U,0U);HAL_NVIC_EnableIRQ(USART1_IRQn);
#endif
}

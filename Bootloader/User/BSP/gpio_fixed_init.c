/* 固定Boot GPIO初始化owner；Phase7/8替换生成gpio.c，其余Target保留HAL基线。
 * Recovery已在此入口之前采样；PA4先拉高后切输出，PB12先选上拉后切输入。
 * 保留其他引脚（含SWD、UART及SPI）的配置，无EXTI/自动介质操作。 */
#include "gpio.h"
void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();__HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOB_CLK_ENABLE();
    GPIOA->BSRR=GPIO_PIN_4;
    MODIFY_REG(GPIOA->CRL,0x000F0000U,0x00020000U);
    GPIOB->BSRR=GPIO_PIN_12;
    MODIFY_REG(GPIOB->CRH,0x000F0000U,0x00080000U);
}

/* 产品DMA初始化：仅DMA1时钟、CH4/5优先级6及IRQ使能，配置顺序同生成owner。 */
#include "stm32f1xx_hal.h"
#include "boot_irq.h"
void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    BootIrq_EnableExternal6(DMA1_Channel4_IRQn);BootIrq_EnableExternal6(DMA1_Channel5_IRQn);
}

/* 产品SysTick/IRQ固定接口owner；CMSIS寄存器访问，厂商源保持只读。
 * HAL公开弱入口仍支持原优先级/频率域和失败顺序，不改变1ms产品时基。 */
#include "stm32f1xx_hal.h"
#include "boot_irq.h"
HAL_StatusTypeDef HAL_InitTick(uint32_t priority)
{
    uint32_t reload=SystemCoreClock/(1000U/uwTickFreq)-1U;
    /* CMSIS同序：LOAD、默认优先级、VAL、CTRL，之后才校验用户优先级。 */
    if(reload>SysTick_LOAD_RELOAD_Msk){return HAL_ERROR;}
    SysTick->LOAD=reload;SCB->SHP[11U]=(uint8_t)(15U<<4U);SysTick->VAL=0U;
    SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_TICKINT_Msk|SysTick_CTRL_ENABLE_Msk;
    if(priority>=16U){return HAL_ERROR;}
    SCB->SHP[11U]=(uint8_t)(BootIrq_Encode(priority)<<4U);uwTickPrio=priority;
    return HAL_OK;
}
void BootIrq_Enable6(IRQn_Type irq)
{
    NVIC_SetPriority(irq,BootIrq_Encode(6U));NVIC_EnableIRQ(irq);
}
void BootIrq_EnableExternal6(uint32_t irq)
{
    /* 显式外部IRQ入口；CMSIS通用/系统异常入口仍保留原语义。
     * Cortex-M3最多240个外部IRQ，越界无副作用；产品只传14/15/37。 */
    if(irq>=240U){return;}
    NVIC->IP[irq]=(uint8_t)(BootIrq_Encode(6U)<<4U);
    NVIC->ISER[irq>>5U]=1UL<<(irq&31U);
}

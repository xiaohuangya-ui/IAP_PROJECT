#ifndef BOOT_IRQ_H
#define BOOT_IRQ_H
#include "stm32f1xx.h"
/* CMSIS NVIC_EncodePriority(group,priority,0)在F103四个优先级位上的
 * 等价式。group0..3不移位，group4..7分别左移1..4，再保留四位。
 * 仍读取当前AIRCR，不假定分组已正确，也不改写分组。 */
static __inline uint32_t BootIrq_Encode(uint32_t priority)
{
    uint32_t group=NVIC_GetPriorityGrouping();
    return (priority<<(group>3U?group-3U:0U))&15U;
}
void BootIrq_Enable6(IRQn_Type irq);
/* 显式外部IRQ专用入口：0..239，非法值拒绝，不接受系统异常。 */
void BootIrq_EnableExternal6(uint32_t irq);
#endif

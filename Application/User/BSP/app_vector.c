#include "stm32f1xx.h"

/* ARMCC5 link substitution: runs before __main and HAL_Init.
 * Do not access initialized globals at this point. */
extern void $Super$$SystemInit(void);

void $Sub$$SystemInit(void)
{
    $Super$$SystemInit();
    SysTick->CTRL = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    SCB->VTOR = 0x08004000U;
    __DSB();
    __ISB();
}

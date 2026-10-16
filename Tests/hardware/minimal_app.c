#include "phase0_entry.h"
#include "phase0_diag.h"
#include "backup_domain.h"
#include "stm32f1xx_hal.h"
/* 独立最小App，不是V1 Release：仅UART、TIM2 Tick及板载LED。
 * 不访问PB12/W25/Metadata，不启动RTOS/ADC/I2C/IWDG，不自动确认。 */
__declspec(noreturn) void Phase0_MinimalApp(void)
{
    uint8_t key;
    uint32_t last = HAL_GetTick();
    if (!P0_Print("P0 APP ALIVE - NOT V1 RELEASE\r\n") ||
        !P0_Field("VTOR=", SCB->VTOR) || !P0_Field("HCLK=", HAL_RCC_GetHCLKFreq()) ||
        !P0_Field("PRIMASK=", __get_PRIMASK()) || !P0_Field("BASEPRI=", __get_BASEPRI()) ||
        !P0_Field("FAULTMASK=", __get_FAULTMASK()) || !P0_Field("CONTROL=", __get_CONTROL()) ||
        !P0_Field("DR3=", Backup_AppReadTrial()) ||
        !P0_Print("P0 APP: u=DR1 test, c=DR2 test, r=reset\r\n")) { P0_Stop(); }
    for (;;) {
        if ((uint32_t)(HAL_GetTick() - last) >= 500U) {
            last = HAL_GetTick();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
        if (!P0_ReadByte(&key)) { continue; }
        if (key == 'u' || key == 'c') {
            /* 用户输入才进行BKP写入；不是Phase9业务健康确认路径。 */
            if (!P0_Print(key == 'u' ? "P0 DR1 TEST RESET\r\n" : "P0 DR2 TEST RESET\r\n") ||
                !P0_FlushTC() || !Backup_AppRequest(key == 'u' ? 1U : 2U)) { P0_Stop(); }
            NVIC_SystemReset();
        } else if (key == 'r') {
            if (!P0_Print("P0 APP RESET\r\n") || !P0_FlushTC()) { P0_Stop(); }
            NVIC_SystemReset();
        }
    }
}

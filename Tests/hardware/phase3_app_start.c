#ifndef PHASE3_APP_START_ONLY
#error Independent App launcher must never enter Release
#endif
#include "boot_entry.h"
#include "phase3_app_identity.h"
#include "recovery.h"
#include "spi_bus.h"
#include "jump.h"
#include "usart.h"
#include <string.h>
/* 仅配套测试App的编译期size/CRC准入；复用正式Jump Prepare/Commit。
 * 不扫描Metadata、不写BKP/Flash/NOR、不启IWDG，不是产品Boot Policy。
 * PB12按住留Boot；错误后停驻，无循环重启或自动擦写。 */
static int hold;
static RecoverySample sample;
static const ImageIdentity trusted = {P3_APP_VERSION, P3_APP_SIZE, P3_APP_CRC32};
static __declspec(noreturn) void stop(void)
{
    __disable_irq(); for (;;) { __WFI(); }
}
static int print(const char *s)
{
    uint32_t start;
    if (HAL_UART_Transmit(&huart1,(uint8_t *)s,(uint16_t)strlen(s),100U)!=HAL_OK) { return 0; }
    start=HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart1,UART_FLAG_TC)==RESET) {
        if ((uint32_t)(HAL_GetTick()-start)>=50U) { return 0; }
    }
    return 1;
}
void Boot_EarlyInit(void) { hold=Recovery_Sample(&sample); }
__declspec(noreturn) void BootMain(void)
{
    JumpTarget target;
    /* 只读检查软件IWDG配置；不改变Option Bytes。Jump重复验证CRC/向量/外设。 */
    if (hold || (FLASH->OBR & FLASH_OBR_IWDG_SW)==0U ||
        HAL_RCC_GetHCLKFreq()!=72000000U ||
        ((GPIOB->CRH>>16)&15U)!=8U || !(GPIOB->ODR&(1U<<12)) ||
        !(GPIOB->IDR&(1U<<12)) || !SpiBus_Init() ||
        !print("P3 APP START TEST ONLY\r\n") || !Jump_Prepare(&trusted,&target)) {
        (void)print("P3 APP START HOLD\r\n");stop();
    }
    if (!print("P3 APP START JUMP\r\n")) { stop(); }
    Jump_Commit(&target);
}

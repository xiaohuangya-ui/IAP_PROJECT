#include "phase0_diag.h"
#include "usart.h"
int P0_FlushTC(void)
{
    uint32_t start = HAL_GetTick();
    while ((USART1->SR & USART_SR_TC) == 0U) {
        if ((uint32_t)(HAL_GetTick() - start) >= 50U) { return 0; }
    }
    return 1;
}
int P0_Print(const char *text)
{
    uint16_t size = 0U;
    while (size < 127U && text[size] != '\0') { ++size; }
    if (text[size] != '\0') { return 0; }
    return HAL_UART_Transmit(&huart1, (uint8_t *)text, size, 100U) == HAL_OK && P0_FlushTC();
}
int P0_Field(const char *name, uint32_t value)
{
    char digits[11];
    uint32_t i;
    static const char hex[] = "0123456789ABCDEF";
    for (i = 0U; i < 8U; ++i) { digits[i] = hex[(value >> (28U - 4U*i)) & 15U]; }
    digits[8] = '\r'; digits[9] = '\n'; digits[10] = '\0';
    return P0_Print(name) && P0_Print(digits);
}
int P0_ReadByte(uint8_t *value)
{
    HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, value, 1U, 2U);
    if (status == HAL_OK) { return 1; }
    /* 空闲超时正常；硬件错误锁停，防止故障输入触发BKP/复位。 */
    if (status != HAL_TIMEOUT || huart1.ErrorCode != HAL_UART_ERROR_NONE) { P0_Stop(); }
    return 0;
}
__declspec(noreturn) void P0_Stop(void)
{
    __disable_irq();
    for (;;) { __WFI(); }
}

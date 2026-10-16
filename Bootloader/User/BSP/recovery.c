#include "recovery.h"
#include "image_contract.h"
#include "stm32f1xx_hal.h"
/* 精确迁移来源：C07_PB12_P1_D2_W25_READONLY_NO_JUMP_TRIAL/
 * User/User_Preflight_BootSafety/c07a_platform.c 的 P1 sampler。
 * 4000 setup / 64 gap / 8000 between 均是 volatile CPU循环次数，非毫秒。
 * 原候选8MHz；本工程72MHz O3重新反汇编/真机验收，历史PASS不继承。 */
static void spin(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0U; i < count; ++i) { __NOP(); }
}
static uint32_t config_ok(void)
{
    return ((RCC->APB2ENR & RCC_APB2ENR_IOPBEN) != 0U &&
            ((GPIOB->CRH >> 16) & 15U) == 8U &&
            (GPIOB->ODR & GPIO_PIN_12) != 0U) ? 1U : 0U;
}
static uint32_t sample16(void)
{
    uint32_t i, high = 0U;
    for (i = 0U; i < 16U; ++i) {
        if ((GPIOB->IDR & GPIO_PIN_12) != 0U) { ++high; }
        spin(64U);
    }
    return high;
}
int Recovery_Sample(RecoverySample *out)
{
    /* #1 先选上拉，再设输入；保留其他针配置。 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    (void)RCC->APB2ENR;
    __DSB();
    GPIOB->ODR |= GPIO_PIN_12;
    GPIOB->CRH = (GPIOB->CRH & ~(15U << 16)) | (8U << 16);
    __DSB();
    /* #2 DWT只用于测量周期，不替代原始采样算法。差值允许u32回绕。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    out->cycle_start = DWT->CYCCNT;
    out->config_pre = config_ok();
    spin(4000U);
    out->high_a = sample16();
    spin(8000U);
    out->high_b = sample16();
    out->config_post = config_ok();
    out->cycle_end = DWT->CYCCNT;
    return Recovery_Hold(out->high_a, out->high_b,
                         out->config_pre & out->config_post);
}

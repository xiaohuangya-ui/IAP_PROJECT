#include "boot_clock.h"
#include "stm32f1xx_hal.h"

/* 同一等待同时使用毫秒和有限轮询；Tick停滞时仍能拒绝启动。
 * 轮询上限不是硬件毫秒保证，不能据此宣称真实启动时限已测。 */
static int wait_bits(volatile uint32_t *reg,uint32_t mask,uint32_t wanted,uint32_t ms)
{
    uint32_t start=HAL_GetTick(),budget=8000000U;
    do {
        if ((*reg&mask)==wanted) { return 1; }
        if ((uint32_t)(HAL_GetTick()-start)>ms) { return 0; }
    } while (--budget!=0U);
    return 0;
}
int BootClock_Init(void)
{
    uint32_t start,budget;
    /* 1. 只接受复位/Jump后HSI上下文。不得在活动PLL上改倍频或HSE旁路。 */
    if (__get_IPSR()!=0U || __get_PRIMASK()!=0U ||
        (RCC->CFGR&(RCC_CFGR_SW|RCC_CFGR_SWS|RCC_CFGR_HPRE))!=0U ||
        (RCC->CR&(RCC_CR_HSIRDY|RCC_CR_HSEBYP|RCC_CR_PLLON|RCC_CR_PLLRDY))!=RCC_CR_HSIRDY ||
        SystemCoreClock!=8000000U) { return 0; }
    RCC->CR|=RCC_CR_HSION|RCC_CR_HSEON;
    if (!wait_bits(&RCC->CR,RCC_CR_HSERDY,RCC_CR_HSERDY,HSE_STARTUP_TIMEOUT)) { return 0; }
    RCC->CSR|=RCC_CSR_LSION; /* 仅振荡器，不写RMVF/BDCR/IWDG。 */
    if (!wait_bits(&RCC->CSR,RCC_CSR_LSIRDY,RCC_CSR_LSIRDY,2U)) { return 0; }
    /* 与HAL一致：LSI Ready后至少1ms稳定；Tick停止也有限退出。 */
    start=HAL_GetTick();budget=8000000U;
    while ((uint32_t)(HAL_GetTick()-start)<2U) { if (--budget==0U) { return 0; } }
    /* 2. 切换前先置Flash2WS/预取与APB分频，绝不短暂超频APB1。 */
    FLASH->ACR=(FLASH->ACR&~FLASH_ACR_LATENCY)|FLASH_ACR_LATENCY_2|FLASH_ACR_PRFTBE;
    if ((FLASH->ACR&FLASH_ACR_LATENCY)!=FLASH_ACR_LATENCY_2) { return 0; }
    RCC->CFGR=(RCC->CFGR&~(RCC_CFGR_PLLSRC|RCC_CFGR_PLLXTPRE|RCC_CFGR_PLLMULL|
        RCC_CFGR_PPRE1|RCC_CFGR_PPRE2|RCC_CFGR_ADCPRE))|
        RCC_CFGR_PLLSRC|RCC_CFGR_PLLMULL9|RCC_CFGR_PPRE1_DIV2|RCC_CFGR_ADCPRE_DIV6;
    RCC->CR|=RCC_CR_PLLON;
    if (!wait_bits(&RCC->CR,RCC_CR_PLLRDY,RCC_CR_PLLRDY,2U)) { return 0; }
    RCC->CFGR=(RCC->CFGR&~RCC_CFGR_SW)|RCC_CFGR_SW_PLL;
    if (!wait_bits(&RCC->CFGR,RCC_CFGR_SWS,RCC_CFGR_SWS_PLL,5000U)) { return 0; }
    /* 3. 真正切换后更新HAL频率与1ms时基；失败不得进入Boot。 */
    SystemCoreClock=72000000U;
    return HAL_InitTick(uwTickPrio)==HAL_OK;
}
void BootClock_Config(void)
{
    extern void Error_Handler(void);
    if (!BootClock_Init()) { Error_Handler(); }
}

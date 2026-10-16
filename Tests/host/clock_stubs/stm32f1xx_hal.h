#include <stdint.h>
typedef struct {volatile uint32_t CR,CFGR,CSR,BDCR;} ClockRcc;
typedef struct {volatile uint32_t ACR;} ClockFlash;
extern ClockRcc rcc;extern ClockFlash flash_regs;
extern uint32_t SystemCoreClock,uwTickPrio,irq,masked;
#define RCC (&rcc)
#define FLASH (&flash_regs)
#define __get_IPSR() irq
#define __get_PRIMASK() masked
#define RCC_CR_HSION 1U
#define RCC_CR_HSIRDY 2U
#define RCC_CR_HSEON 0x10000U
#define RCC_CR_HSERDY 0x20000U
#define RCC_CR_HSEBYP 0x40000U
#define RCC_CR_PLLON 0x1000000U
#define RCC_CR_PLLRDY 0x2000000U
#define RCC_CFGR_SW 3U
#define RCC_CFGR_SWS 12U
#define RCC_CFGR_SW_PLL 2U
#define RCC_CFGR_SWS_PLL 8U
#define RCC_CFGR_HPRE 0xF0U
#define RCC_CFGR_PPRE1 0x700U
#define RCC_CFGR_PPRE2 0x3800U
#define RCC_CFGR_PPRE1_DIV2 0x400U
#define RCC_CFGR_ADCPRE 0xC000U
#define RCC_CFGR_ADCPRE_DIV6 0x8000U
#define RCC_CFGR_PLLSRC 0x10000U
#define RCC_CFGR_PLLXTPRE 0x20000U
#define RCC_CFGR_PLLMULL 0x3C0000U
#define RCC_CFGR_PLLMULL9 0x1C0000U
#define RCC_CSR_LSION 1U
#define RCC_CSR_LSIRDY 2U
#define FLASH_ACR_LATENCY 7U
#define FLASH_ACR_LATENCY_2 2U
#define FLASH_ACR_PRFTBE 16U
#define HSE_STARTUP_TIMEOUT 100U
#define HAL_OK 0
uint32_t HAL_GetTick(void);
int HAL_InitTick(uint32_t priority);

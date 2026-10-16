#ifndef LSI_HOST_HAL_H
#define LSI_HOST_HAL_H
#include <stdint.h>
typedef struct {volatile uint32_t CR,CFGR,BDCR,CSR,APB1ENR;} RccMock;
typedef struct {volatile uint32_t CR;} PwrMock;
typedef struct {volatile uint32_t CRL,PRLH,PRLL,CNTH,CNTL,DIVH,DIVL;} RtcMock;
typedef struct {volatile uint32_t OBR;} FlashMock;
typedef struct {volatile uint32_t KR,PR,RLR,SR;} IwdgMock;
extern RccMock rcc;extern PwrMock pwr;extern RtcMock rtc;extern FlashMock flash_regs;extern IwdgMock iwdg;
#define RCC (&rcc)
#define PWR (&pwr)
#define RTC (&rtc)
#define FLASH (&flash_regs)
#define IWDG (&iwdg)
#define RCC_CR_HSERDY (1U<<17)
#define RCC_CFGR_PLLSRC (1U<<16)
#define RCC_CFGR_SWS 12U
#define RCC_CFGR_SWS_PLL 8U
#define RCC_BDCR_RTCSEL (3U<<8)
#define RCC_BDCR_RTCSEL_1 (2U<<8)
#define RCC_BDCR_RTCEN (1U<<15)
#define RCC_APB1ENR_PWREN (1U<<28)
#define RCC_APB1ENR_BKPEN (1U<<27)
#define PWR_CR_DBP (1U<<8)
#define RCC_CSR_LSION 1U
#define RCC_CSR_LSIRDY 2U
#define RTC_CRL_CNF 16U
#define RTC_CRL_RSF 8U
#define RTC_CRL_RTOFF 32U
#define RTC_CRL_OWF 4U
#define RTC_CRL_ALRF 2U
#define RTC_CRL_SECF 1U
#define FLASH_OBR_IWDG_SW 4U
extern uint32_t irq,clock_hz;
#define __get_IPSR() irq
#define __get_PRIMASK() 0U
#define __get_BASEPRI() 0U
#define __get_FAULTMASK() 0U
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetHCLKFreq(void);
#endif

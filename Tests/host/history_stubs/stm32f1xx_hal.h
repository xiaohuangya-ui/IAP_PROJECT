#ifndef HISTORY_HAL_STUB
#define HISTORY_HAL_STUB
#include <stdint.h>
#define __IO volatile
typedef struct {uint32_t CRL,CRH,IDR,ODR,BSRR,BRR;} HGPIO;
typedef struct {uint32_t CR1,CR2,SR,DR,I2SCFGR;} HSPI;
typedef struct {uint32_t CFGR,APB2ENR,APB2RSTR;} HRCC;
typedef struct {uint32_t MAPR;} HAFIO;
extern HAFIO h_afio;
#define AFIO (&h_afio)
#define AFIO_MAPR_SPI1_REMAP 1U
#define RCC_APB2ENR_AFIOEN 1U
extern HGPIO h_gpio;extern HSPI h_spi;extern HRCC h_rcc;
#define GPIOA (&h_gpio)
#define SPI1 (&h_spi)
#define RCC (&h_rcc)
#define GPIO_PIN_4 (1U<<4)
#define RCC_CFGR_PPRE2 (7U<<11)
#define RCC_APB2ENR_IOPAEN (1U<<2)
#define RCC_APB2ENR_SPI1EN (1U<<12)
#define RCC_APB2RSTR_SPI1RST (1U<<12)
#define SPI_SR_OVR (1U<<6)
#define SPI_SR_MODF (1U<<5)
#define SPI_SR_TXE (1U<<1)
#define SPI_SR_RXNE (1U<<0)
#define SPI_SR_BSY (1U<<7)
#define SPI_CR1_CPOL (1U<<1)
#define SPI_CR1_CPHA (1U<<0)
#define SPI_CR1_MSTR (1U<<2)
#define SPI_CR1_BR_0 (1U<<3)
#define SPI_CR1_BR_1 (1U<<4)
#define SPI_CR1_SSM (1U<<9)
#define SPI_CR1_SSI (1U<<8)
#define SPI_CR1_SPE (1U<<6)
extern uint32_t SystemCoreClock;
uint32_t HAL_GetTick(void);
uint32_t __get_IPSR(void);uint32_t __get_PRIMASK(void);uint32_t __get_BASEPRI(void);
#endif

#ifndef HOST_STM32_H
#define HOST_STM32_H
/* 仅主机寄存器夹具；不得加入任一Keil工程。 */
#include <stdint.h>
typedef struct { volatile uint32_t CR1,CR2,SR,DR; } SPI_TypeDef;
typedef struct { volatile uint32_t CRL,ODR; } GPIO_TypeDef;
typedef struct { volatile uint32_t APB1ENR,BDCR,CSR,CR,CFGR; } RCC_TypeDef;
extern uint32_t SystemCoreClock;
#define RCC_CFGR_PPRE2_2 (1U<<13)
typedef struct { volatile uint32_t CR; } PWR_TypeDef;
typedef struct { volatile uint32_t DR1,DR2,DR3; } BKP_TypeDef;
extern SPI_TypeDef host_spi;
extern GPIO_TypeDef host_gpio;
extern RCC_TypeDef host_rcc;
extern PWR_TypeDef host_pwr;
extern BKP_TypeDef host_bkp;
#define SPI1 (&host_spi)
#define GPIOA (&host_gpio)
#define RCC (&host_rcc)
#define PWR (&host_pwr)
#define BKP (&host_bkp)
#define SPI_CR1_MSTR (1U<<2)
#define SPI_CR1_CPOL (1U<<1)
#define SPI_CR1_CPHA 1U
#define SPI_CR1_BR (7U<<3)
#define SPI_CR1_BR_0 (1U<<3)
#define SPI_CR1_BR_1 (1U<<4)
#define SPI_CR1_DFF (1U<<11)
#define SPI_CR1_SSM (1U<<9)
#define SPI_CR1_SSI (1U<<8)
#define SPI_CR1_SPE (1U<<6)
#define SPI_CR1_LSBFIRST (1U<<7)
#define SPI_CR1_RXONLY (1U<<10)
#define SPI_CR1_CRCEN (1U<<13)
#define SPI_CR1_BIDIMODE (1U<<15)
#define SPI_SR_BSY (1U<<7)
#define SPI_SR_RXNE 1U
#define SPI_SR_OVR (1U<<6)
#define SPI_SR_MODF (1U<<5)
#define SPI_SR_TXE (1U<<1)
#define RCC_APB1ENR_PWREN (1U<<28)
#define RCC_APB1ENR_BKPEN (1U<<27)
#define PWR_CR_DBP (1U<<8)
extern uint32_t host_ipsr,host_primask,host_basepri,host_faultmask,host_control;
#define __get_IPSR() host_ipsr
#define __get_PRIMASK() host_primask
#define __get_BASEPRI() host_basepri
#define __get_FAULTMASK() host_faultmask
#define __get_CONTROL() host_control
#define __DSB() ((void)0)
#define __NOP() ((void)0)
typedef struct { volatile uint32_t SR; } USART_TypeDef;
typedef struct { volatile uint32_t CCR; } DMA_Channel_TypeDef;
typedef struct { volatile uint32_t IABR[2]; } NVIC_TypeDef;
extern USART_TypeDef host_usart;
extern DMA_Channel_TypeDef host_dma4,host_dma5;
extern NVIC_TypeDef host_nvic;
#define USART1 (&host_usart)
#define DMA1_Channel4 (&host_dma4)
#define DMA1_Channel5 (&host_dma5)
#define NVIC (&host_nvic)
#define USART_SR_TC (1U<<6)
#define DMA_CCR_EN 1U
#define RCC_CR_HSIRDY (1U<<1)
#endif

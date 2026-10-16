#ifndef FIXTURE_HAL_H
#define FIXTURE_HAL_H
#include <stdint.h>
#define __declspec(x) __attribute__((x))
uint32_t HAL_GetTick(void);
typedef struct { volatile uint32_t CRH, IDR; } FixtureGPIO;
typedef struct { volatile uint32_t CR1, CR3; } FixtureUSART;
extern FixtureGPIO fixture_gpioa;
extern FixtureUSART fixture_usart1;
#define GPIOA (&fixture_gpioa)
#define USART1 (&fixture_usart1)
#define GPIO_PIN_10 (1U << 10)
typedef struct { volatile uint32_t CSR; } FixtureRCC;
extern FixtureRCC fixture_rcc;
#define RCC (&fixture_rcc)
/* 模拟RMVF清复位标志的效果；低位LSI配置/状态保持。 */
#define __HAL_RCC_CLEAR_RESET_FLAGS() (fixture_rcc.CSR &= ~0xFC000000U)
#endif

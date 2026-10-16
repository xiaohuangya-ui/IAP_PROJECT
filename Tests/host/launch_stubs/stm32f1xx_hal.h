#ifndef LAUNCH_HAL_H
#define LAUNCH_HAL_H
#include <stdint.h>
#define __declspec(x) __attribute__((x))
typedef struct { uint32_t OBR; } TestFlash;
typedef struct { uint32_t CRH,ODR,IDR; } TestGPIO;
extern TestFlash test_flash;
extern TestGPIO test_gpio;
#define FLASH (&test_flash)
#define GPIOB (&test_gpio)
#define FLASH_OBR_IWDG_SW 4U
#define HAL_OK 0
#define RESET 0
#define UART_FLAG_TC 64U
extern int test_tc;
#define __HAL_UART_GET_FLAG(u,f) test_tc
void __disable_irq(void);
void __WFI(void);
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetHCLKFreq(void);
int HAL_UART_Transmit(void *u,uint8_t *p,uint16_t n,uint32_t ms);
#endif

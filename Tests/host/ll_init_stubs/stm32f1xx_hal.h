#ifndef LL_INIT_MODEL_H
#define LL_INIT_MODEL_H
#include "ll_device_model.h"
typedef struct {uint32_t Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
typedef struct {uint32_t CRH,ODR,BSRR;} GPIO_TypeDef;
extern GPIO_TypeDef init_gpio;
#define GPIOA (&init_gpio)
#define GPIO_PIN_9 (1U<<9)
#define GPIO_PIN_10 (1U<<10)
#define GPIO_MODE_AF_PP 2U
#define GPIO_MODE_INPUT 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_HIGH 3U
#define USART1_IRQn 37U
extern uint32_t SystemCoreClock;
uint32_t HAL_RCC_GetPCLK2Freq(void);
void InitModel_EnableClock(unsigned bit);
#define __HAL_RCC_USART1_CLK_ENABLE() InitModel_EnableClock(1U)
#define __HAL_RCC_GPIOA_CLK_ENABLE() InitModel_EnableClock(2U)
void HAL_GPIO_Init(GPIO_TypeDef *gpio,const GPIO_InitTypeDef *config);
void HAL_NVIC_SetPriority(unsigned irq,unsigned priority,unsigned sub);
void HAL_NVIC_EnableIRQ(unsigned irq);
#define MODIFY_REG(reg,mask,value) ((reg)=((reg)&~(mask))|(value))
#endif

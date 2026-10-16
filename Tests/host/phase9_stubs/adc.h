#ifndef P9_STUB_ADC
#define P9_STUB_ADC
#include "stm32f1xx.h"
typedef int ADC_HandleTypeDef;
extern ADC_HandleTypeDef hadc1;
#define HAL_OK 0
#define GPIOC 3
#define GPIO_PIN_13 8192
void MX_ADC1_Init(void);
int HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a);
int HAL_ADC_Start(ADC_HandleTypeDef *a);
int HAL_ADC_Stop(ADC_HandleTypeDef *a);
int HAL_ADC_PollForConversion(ADC_HandleTypeDef *a,uint32_t timeout);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a);
void HAL_Delay(uint32_t ms);
uint32_t HAL_GetTick(void);
void HAL_GPIO_TogglePin(int port,int pin);
#endif

#include "stm32f1xx_hal.h"
#define LL_GPIO_PIN_10 ((1U<<18)|0x04000004U)
void InitModel_SetOutputPin(GPIO_TypeDef *gpio,uint32_t mask);
#define LL_GPIO_SetOutputPin(gpio,mask) InitModel_SetOutputPin((gpio),(mask))

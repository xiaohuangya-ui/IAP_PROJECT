#include "stm32f1xx_hal.h"
/* 模型只重现ST的直接寄存器写宏；初始化C本体是真实被测源码。 */
#define LL_USART_WriteReg(instance,reg,value) ((instance)->reg=(value))

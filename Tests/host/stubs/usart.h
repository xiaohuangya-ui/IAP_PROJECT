#include "stm32f1xx.h"
typedef struct { uint32_t gState,RxState; } UART_HandleTypeDef;
extern UART_HandleTypeDef huart1;
#define HAL_UART_STATE_READY 0x20U

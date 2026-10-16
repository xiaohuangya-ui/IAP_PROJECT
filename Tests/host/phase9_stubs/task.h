#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
void vTaskDelayUntil(TickType_t *last,TickType_t ticks);
uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t handle);

void xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(int clear,TickType_t ticks);

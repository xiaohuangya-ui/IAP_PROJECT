#ifndef PHASE9_STUB_RTOS
#define PHASE9_STUB_RTOS
#include <stdint.h>
#include <stddef.h>
typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void *);
typedef void *TaskHandle_t;
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define taskENTER_CRITICAL() test_critical(1)
#define taskEXIT_CRITICAL() test_critical(-1)
void test_critical(int delta);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
#endif

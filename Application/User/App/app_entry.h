#ifndef APP_ENTRY_H
#define APP_ENTRY_H

#include "FreeRTOS.h"
#include "task.h"

__declspec(noreturn) void App_Main(void);
__declspec(noreturn) void App_FatalStop(void);
/* Phase9 supplies real business functions after its implementation gate.
 * Stack sizes are words; allocation comes from heap_4. */
__declspec(noreturn) void App_StartNativeTasks(TaskFunction_t uart_recv,
                                             TaskFunction_t upgrade_ctrl,
                                             TaskFunction_t business);

#endif

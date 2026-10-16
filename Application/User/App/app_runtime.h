#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H
#include "FreeRTOS.h"
#include "task.h"
/* 注册器在启动调度器前填写；通知只唤醒，接收任务仍复核受保护状态。 */
extern TaskHandle_t app_uart_task,app_upgrade_task,app_business_task;
__declspec(noreturn) void AppRuntime_Run(void);
#endif

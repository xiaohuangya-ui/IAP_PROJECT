#include "app_entry.h"
#include "stm32f1xx.h"
#ifdef APP_PHASE9
#include "app_runtime.h"
#endif
#if !defined(PHASE0_TEST_ONLY)
#include "app_shell.h"
#endif
#ifdef PHASE0_TEST_ONLY
#include "phase0_entry.h"
#endif

__declspec(noreturn) void App_FatalStop(void)
{
    __disable_irq();
    for (;;) { __WFI(); }
}

__declspec(noreturn) void App_Main(void)
{
#ifdef PHASE0_TEST_ONLY
    Phase0_MinimalApp();
#elif defined(APP_PHASE9)
    AppRuntime_Run();
#else
    /* Phase3仅维护Shell，业务/健康状态尚未就绪；不启动生成任务或写BKP。 */
    AppShell_Run();
    App_FatalStop();
#endif
}

__declspec(noreturn) void App_StartNativeTasks(TaskFunction_t uart_recv,
                                             TaskFunction_t upgrade_ctrl,
                                             TaskFunction_t business)
{
    static uint8_t registration_started;
    TaskHandle_t *uart_handle=NULL,*ctrl_handle=NULL,*business_handle=NULL;
#ifdef APP_PHASE9
    uart_handle=&app_uart_task;ctrl_handle=&app_upgrade_task;business_handle=&app_business_task;
#endif
    if ((registration_started != 0U) || (uart_recv == NULL) ||
        (upgrade_ctrl == NULL) || (business == NULL) ||
        (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)) {
        App_FatalStop();
    }
    registration_started = 1U;
    if (xTaskCreate(uart_recv, "Task_UartRecv", 320U, NULL, 3U, uart_handle) != pdPASS) {
        App_FatalStop();
    }
    if (xTaskCreate(upgrade_ctrl, "Task_UpgradeCtrl", 192U, NULL, 2U, ctrl_handle) != pdPASS) {
        App_FatalStop();
    }
    if (xTaskCreate(business, "Task_Business", 320U, NULL, 1U, business_handle) != pdPASS) {
        App_FatalStop();
    }
    vTaskStartScheduler();
    App_FatalStop();
}

/* Strong definitions replace CubeMX's empty weak failure hooks. */
void vApplicationMallocFailedHook(void)
{
    App_FatalStop();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    App_FatalStop();
}

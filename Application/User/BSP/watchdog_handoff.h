#ifndef APP_WATCHDOG_HANDOFF_H
#define APP_WATCHDOG_HANDOFF_H
#include "watchdog_contract.h"
/* Phase8接力，绝不写PR/RLR或CCCC；调用前必须有明确Boot已启动上下文。
 * Early最多一次；Healthy必须有新真实业务进展，不能由升级控制任务自增。
 * Phase9真实业务尚未接入；测试目标明确隔离。 */
void AppWatchdog_Init(WatchdogHealth *state,uint32_t now,uint32_t business);
int AppWatchdog_Early(WatchdogHealth *state);
int AppWatchdog_Healthy(WatchdogHealth *state,uint32_t now,uint32_t business,int uart_ready,int fatal);
#endif

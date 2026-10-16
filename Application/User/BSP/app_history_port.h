#ifndef APP_HISTORY_PORT_H
#define APP_HISTORY_PORT_H
/* 仅Business在AppHistory_Bind之前取端口；无初始化或介质副作用。 */
#include "app_history.h"
const AppHistoryPort *AppHistory_DevicePort(void);
#endif

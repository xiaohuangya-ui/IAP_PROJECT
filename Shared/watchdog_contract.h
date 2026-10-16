#ifndef WATCHDOG_CONTRACT_H
#define WATCHDOG_CONTRACT_H
#include <stdint.h>
/* 纯算法，Prescaler=64，按实测频率四舍五入4s，非标称40kHz。 */
int Watchdog_Reload(uint32_t measured_hz,uint16_t *reload);
typedef struct {uint32_t started_ms,last_feed_ms,last_business;uint8_t handoff_used;} WatchdogHealth;
/* 只给出刷新资格，不写IWDG；业务进度来自唯一业务owner，UART空闲可Ready。 */
int Watchdog_Healthy(WatchdogHealth *s,uint32_t now,uint32_t business,int uart_ready,int fatal);
#endif

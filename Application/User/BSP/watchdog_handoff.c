#include "watchdog_handoff.h"
#include "stm32f1xx.h"
void AppWatchdog_Init(WatchdogHealth *s,uint32_t now,uint32_t business)
{s->started_ms=s->last_feed_ms=now;s->last_business=business;s->handoff_used=0;}
int AppWatchdog_Early(WatchdogHealth *s)
{
    if(!s||s->handoff_used||__get_IPSR()){return 0;}
    s->handoff_used=1;IWDG->KR=0xAAAAU;return 1;
}
int AppWatchdog_Healthy(WatchdogHealth *s,uint32_t now,uint32_t business,int ready,int fatal)
{
    if(__get_IPSR()||!Watchdog_Healthy(s,now,business,ready,fatal)){return 0;}
    IWDG->KR=0xAAAAU;return 1;
}

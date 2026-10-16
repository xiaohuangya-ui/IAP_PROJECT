#include "watchdog_contract.h"
int Watchdog_Reload(uint32_t hz,uint16_t *reload)
{
    uint32_t ticks;
    if(!reload||(uint32_t)(hz-30000U)>30000U){return 0;}
    ticks=(hz+8U)/16U; /* round(hz*4/64)，避免乘法溢出。 */
    /* 已验证hz范围，ticks必为1875..3750，原夹紧分支不可达。 */
    *reload=(uint16_t)(ticks-1U);return 1;
}
int Watchdog_Healthy(WatchdogHealth *s,uint32_t now,uint32_t business,int uart_ready,int fatal)
{
    uint32_t delta;
    if(!s||!uart_ready||fatal||(uint32_t)(now-s->last_feed_ms)>2000U){return 0;}
    delta=business-s->last_business;
    if(!delta||delta>=0x80000000U){return 0;}
    s->last_business=business;s->last_feed_ms=now;return 1;
}

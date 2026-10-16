/* 测试链接拦截只计数并调用原实现；禁止加入Release、ISR打印或替换DMA owner。 */
#ifndef PHASE34_RAM_TRACE_ONLY
#error RAM trace is an isolated test target only
#endif
#include "uart_port.h"
#include "phase34_ram_trace.h"
#include "usart.h"
#include <string.h>
#ifdef TRACE_HOST_TEST
#define REAL(n) Mock_##n
#define HOOK(n) Trace_##n
extern uint32_t SystemCoreClock;
#else
#define REAL(n) $Super$$##n
#define HOOK(n) $Sub$$##n
#endif
extern int REAL(Uart_Start)(void);
extern void REAL(Uart_Service)(void);
extern int REAL(Uart_ReadByte)(uint8_t *);
extern int REAL(Uart_TxSubmit)(const uint8_t *,uint16_t);
extern uint32_t REAL(Uart_TakeErrors)(void);
extern void REAL(BootProtocol_Init)(void);
extern void REAL(BootProtocol_Poll)(void);
typedef char TraceSizeCheck[(sizeof(TraceSnapshot)==96U)?1:-1];
volatile TraceSnapshot g_phase34_ram;
static uint32_t inits,polls,services,starts,start_result,consumed,accepted,seen,last_publish;
static uint32_t context(void)
{
    return (__get_IPSR()?1U:0U)|(__get_PRIMASK()?2U:0U)|
           (__get_BASEPRI()?4U:0U)|(__get_FAULTMASK()?8U:0U);
}
static void publish(void)
{
    UartStats s;uint32_t bits=context();
    memset(&s,0,sizeof(s));
    /* 前台计数不由ISR修改。GetStats锁内取owner快照；本轮前台计数在该时点
       不变，复制后ISR可继续收发，但本快照仍表示该采样点，非持续实时值。 */
    if(!bits){Uart_GetStats(&s);}
    g_phase34_ram.seq++;__DMB();
    g_phase34_ram.magic=0x5034524DU;g_phase34_ram.abi=1U;
    g_phase34_ram.init_calls=inits;g_phase34_ram.poll_calls=polls;
    g_phase34_ram.service_calls=services;g_phase34_ram.start_calls=starts;
    g_phase34_ram.start_result=start_result;g_phase34_ram.context_bits=bits;
    g_phase34_ram.tick_ms=HAL_GetTick();g_phase34_ram.core_clock_hz=SystemCoreClock;
    g_phase34_ram.stats_valid=(bits==0U);
    g_phase34_ram.rx_collected=s.received_bytes;g_phase34_ram.rx_consumed=consumed;
    g_phase34_ram.tx_accepted=accepted;g_phase34_ram.tx_tc_completed=s.transmitted_bytes;
    g_phase34_ram.discarded=s.discarded_bytes;g_phase34_ram.errors_total=s.total_errors;
    g_phase34_ram.error_bits_seen=seen|s.pending_errors;g_phase34_ram.pending_errors=s.pending_errors;
    g_phase34_ram.recoveries=s.recoveries;g_phase34_ram.queued=s.queued_bytes;
    g_phase34_ram.rx_ready=s.rx_ready;g_phase34_ram.tx_busy=s.tx_busy;
    __DMB();g_phase34_ram.seq++;
}
int HOOK(Uart_Start)(void)
{int result;starts++;result=REAL(Uart_Start)();start_result=(uint32_t)result;publish();return result;}
void HOOK(Uart_Service)(void)
{
    uint32_t now;services++;REAL(Uart_Service)();now=HAL_GetTick();
    /* HAL tick未前进时仍每4096次Service发布，避免把旧快照误当主循环停驻。 */
    if((uint32_t)(now-last_publish)>=10U || !(services&4095U)){last_publish=now;publish();}
}
int HOOK(Uart_ReadByte)(uint8_t *b)
{int result=REAL(Uart_ReadByte)(b);if(result){consumed++;}return result;}
int HOOK(Uart_TxSubmit)(const uint8_t *p,uint16_t n)
{int result=REAL(Uart_TxSubmit)(p,n);if(result){accepted+=n;}return result;}
uint32_t HOOK(Uart_TakeErrors)(void)
{uint32_t errors=REAL(Uart_TakeErrors)();seen|=errors;return errors;}
void HOOK(BootProtocol_Init)(void)
{inits++;REAL(BootProtocol_Init)();publish();}
void HOOK(BootProtocol_Poll)(void)
{polls++;REAL(BootProtocol_Poll)();}

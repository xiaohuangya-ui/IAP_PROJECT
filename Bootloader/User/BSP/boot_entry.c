#include "boot_entry.h"
#include "stm32f1xx.h"
#include "uart_port.h"
#ifndef PHASE0_TEST_ONLY
#ifndef PHASE1_TEST_ONLY
#ifndef PHASE2_TEST_ONLY
#include "boot_protocol.h"
#endif
#endif
#endif

#ifdef PHASE0_TEST_ONLY
#include "phase0_entry.h"
#endif
#ifdef PHASE2_TEST_ONLY
__declspec(noreturn) void Phase2_Main(void);
#endif
#ifdef PHASE4_TEST_ONLY
void Phase4_Main(void);
#endif
#ifdef PHASE1_TEST_ONLY
__declspec(noreturn) void Phase1_UartMain(void);
#endif
void Boot_EarlyInit(void)
{
#ifdef PHASE0_TEST_ONLY
    Phase0_BootEarly();
#endif
}
__declspec(noreturn) void BootMain(void)
{
#ifdef PHASE0_TEST_ONLY
    Phase0_BootMain();
#elif defined(PHASE1_TEST_ONLY)
    Phase1_UartMain();
#elif defined(PHASE2_TEST_ONLY)
    Phase2_Main();
#elif defined(PHASE4_TEST_ONLY)
    Phase4_Main();
    for (;;) { }
#else
    /* Phase3维护命令真实可达；未知持久状态不启动App或IWDG。 */
    BootProtocol_Run();
    for (;;) { }
#endif
}

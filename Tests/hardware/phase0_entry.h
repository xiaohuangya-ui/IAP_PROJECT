#ifndef PHASE0_ENTRY_H
#define PHASE0_ENTRY_H
#ifndef PHASE0_TEST_ONLY
#error Phase0 entry is test-only
#endif
void Phase0_BootEarly(void);
__declspec(noreturn) void Phase0_BootMain(void);
__declspec(noreturn) void Phase0_MinimalApp(void);
#endif

#ifndef PHASE0_DIAG_H
#define PHASE0_DIAG_H
#include <stdint.h>
#ifndef PHASE0_TEST_ONLY
#error Phase0 diagnostics must never be compiled in Release
#endif
/* 仅测试target的前台阻塞UART，发送超时100ms，硬件TC超时50ms。
 * 不启动DMA/IDLE、不变更厂商IRQ owner；失败返回0，不允许继续Jump/复位握手。 */
int P0_Print(const char *text);
int P0_Field(const char *name, uint32_t value);
int P0_FlushTC(void);
int P0_ReadByte(uint8_t *value);
__declspec(noreturn) void P0_Stop(void);
#endif

/* 实际Phase2夹具负例：HAL提交拒绝一次时，已消费的回显字节必须保留。 */
#include "uart_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
static jmp_buf done;
static unsigned pos, calls, sent, mode;
static uint8_t output[513];
void Phase2_Main(void);
uint32_t HAL_GetTick(void) { return 0U; }
int Uart_Start(void) { return 1; }
void Uart_Service(void) {
    if (++calls > 5000U) { longjmp(done, 1); }
}
int Uart_TxBusy(void) { return 0; }
uint32_t Uart_TakeErrors(void) { return 0U; }
int Uart_ReadByte(uint8_t *b) {
    if (!pos) { pos++; *b = 'e'; return 1; }
    if (pos > sizeof(output)) { return 0; }
    *b = (uint8_t)(pos++ - 1U); return 1;
}
int Uart_TxSubmit(const uint8_t *p, uint16_t n) {
    if (n == 21U && !memcmp(p, "P2 DMA TEST", 11U)) { return 1; }
    if (mode) { mode = 0U; return 0; }
    if (n > sizeof(output) - sent) { exit(3); }
    memcpy(output + sent, p, n); sent += n; return 1;
}
int Uart_FlushTC(uint32_t ms) { (void)ms; return 1; }
int Uart_RxFence(void) { return 1; }
void Uart_GetStats(UartStats *s) { memset(s, 0, sizeof(*s)); }
int main(int argc, char **argv) {
    unsigned i; mode = argc > 1 ? (unsigned)atoi(argv[1]) : 0U;
    if (!setjmp(done)) { Phase2_Main(); }
    if (sent != sizeof(output)) { fprintf(stderr, "lost ownership: %u/513\n", sent); return 2; }
    for (i = 0U; i < sent; ++i) { if (output[i] != (uint8_t)i) { return 4; } }
    puts("{\"result\":\"PASS\",\"bytes\":513}"); return 0;
}

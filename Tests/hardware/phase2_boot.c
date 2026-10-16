#ifndef PHASE2_TEST_ONLY
#error Test fixture must not enter Release
#endif
#include "uart_port.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>
/* 隔离DMA夹具：e切换二进制回显，s状态，t发送256B并等真实TC，h暂停消费触发溢出。
 * 无Flash/NOR/BKP写入，无复位/IWDG/跳转，也无历史根因探针。 */
__declspec(noreturn) void Phase2_Main(void)
{
    uint8_t data[256], b;
    uint16_t n = 0U;
    uint32_t hold = 0U;
    uint32_t last_errors = 0U,errors;
    int echo = 0,holding = 0;
    UartStats stats;
    char line[160];
    (void)Uart_Start();
    (void)Uart_TxSubmit((const uint8_t *)"P2 DMA TEST e/s/t/h\r\n",21U);
    for (;;) {
        Uart_Service();
        if (Uart_TxBusy()) { continue; }
        errors=Uart_TakeErrors();if(errors){last_errors|=errors;echo=0;n=0U;}
        if (holding) {
            if ((uint32_t)(HAL_GetTick() - hold) < 500U) { continue; }
            /* 洪泛仅用于溢出单元；恢复后的残留不能作为夹具控制字符执行。 */
            holding=0;(void)Uart_RxFence();
        }
        if (echo) {
            /* 前台拥有已消费数据，提交拒绝后保留，不能继续消费或覆盖。
             * 真实UART故障仍由上面的错误分支显式退出回显，不假装成功。 */
            if (!n) {
                while (n < 256U && Uart_ReadByte(&b)) { data[n++] = b; }
            }
            if (n && Uart_TxSubmit(data,n)) { n = 0U; }
        } else if (Uart_ReadByte(&b)) {
            if (b == 'e') { echo = 1; n = 0U; }
            else if (b == 'h') { hold = HAL_GetTick();holding=1; }
            else if (b == 't') {
                for (n = 0; n < 256U; ++n) { data[n] = (uint8_t)n; }
                if (Uart_TxSubmit(data,256U) && Uart_FlushTC(100U)) {
                    (void)Uart_TxSubmit((const uint8_t *)"P2 TC OK\r\n",10U);
                }
            } else if (b == 's') {
                Uart_GetStats(&stats);
                n = (uint16_t)snprintf(line,sizeof(line),"P2 RX=%lu DROP=%lu ERR=%lu RECOVER=%lu QUEUE=%u BITS=%lu\r\n",
                    (unsigned long)stats.received_bytes,(unsigned long)stats.discarded_bytes,
                    (unsigned long)stats.total_errors,(unsigned long)stats.recoveries,stats.queued_bytes,(unsigned long)last_errors);
                if (n < sizeof(line)) { (void)Uart_TxSubmit((const uint8_t *)line,n); }
            }
        }
    }
}

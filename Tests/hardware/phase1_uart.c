/* 独立Phase1测试入口：普通UART/Ring/TC，无跳转、BKP、Flash/W25写擦、IWDG。 */
#ifndef PHASE1_TEST_ONLY
#error Phase1 fixture must never enter Release
#endif
#include "uart_port.h"
#include "stm32f1xx_hal.h"
#include "image_contract.h"
#include "phase1_tx_vector.h"
#include <string.h>
static int send(const uint8_t *data, uint16_t count)
{
    return Uart_TxSubmit(data, count) && Uart_FlushTC(100U);
}
static void say(const char *text) { (void)send((const uint8_t *)text,(uint16_t)strlen(text)); }
static void number(uint32_t value)
{
    uint8_t b[8];
    unsigned i;
    for (i=0U;i<8U;i++) {
        unsigned digit=(unsigned)((value >> ((7U-i)*4U)) & 15U);
        b[i]=(uint8_t)(digit<10U ? '0'+digit : 'A'+digit-10U);
    }
    (void)send(b,8U);
}
static void report(uint32_t errors)
{
    UartStats s;
    Uart_GetStats(&s);
    say("P1 ERR="); number(errors);
    say(" DROP="); number(s.discarded_bytes);
    say(" RECOVER="); number(s.recoveries);
    say(" RX="); number(s.received_bytes);
    say(" TX="); number(s.transmitted_bytes);
    say(" QUEUE="); number(s.queued_bytes);
    say("\r\n");
}
/* 简介：前台10秒只读观察PA10，UART仍由原HAL IT owner收发/恢复。
 * 每个不同的HAL tick最多采样一次；LOW/HIGH是采样次数，不是连续脉宽。
 * 不读取USART DR/SR、不清标志、不改GPIO或IRQ配置；窗口内输入丢弃，
 * 避免探针期间收到的字节在窗口结束后被当成控制命令。
 */
static void pin_probe(void)
{
    uint32_t start, last, now, low=0U, high=0U, errors=0U;
    uint32_t crh=GPIOA->CRH, cr1=USART1->CR1, cr3=USART1->CR3;
    UartStats before, after;
    uint8_t discard;
    uint16_t drained;
    say("P1 PIN BEGIN 10s; disconnect TTL TXD before grounding PA10\r\n");
    Uart_GetStats(&before);
    start=HAL_GetTick(); last=start-1U;
    for (;;) {
        Uart_Service();
        errors |= Uart_TakeErrors();
        /* 每轮有界清空，下一轮仍先检查10秒截止时间。 */
        for (drained=0U;drained<128U && Uart_ReadByte(&discard);drained++) { }
        now=HAL_GetTick();
        if ((uint32_t)(now-start)>=10000U) { break; }
        if (now!=last) {
            last=now;
            if ((GPIOA->IDR & GPIO_PIN_10)==0U) { low++; }
            else { high++; }
        }
    }
    Uart_GetStats(&after);
    say("P1 PIN LOW="); number(low);
    say(" HIGH="); number(high);
    say(" CRH="); number(crh);
    say(" CR1="); number(cr1);
    say(" CR3="); number(cr3);
    say(" ERR="); number(errors);
    say(" TOTAL="); number(after.total_errors-before.total_errors);
    say(" RXDELTA="); number(after.received_bytes-before.received_bytes);
    say(" RECOVER="); number(after.recoveries-before.recoveries);
    say("\r\nP1 PIN END\r\n");
}
__declspec(noreturn) void Phase1_UartMain(void)
{
    uint32_t reset_csr=RCC->CSR;
    uint8_t byte, block[128];
    uint16_t count;
    uint32_t errors, until;
    int echo=0;
    int diagnostic=0;
    int receive_only=0;
    uint32_t diag_crc=0xFFFFFFFFU, diag_forward=0U, diag_tx=0U;
    uint32_t diag_errors=0U, diag_last=0U, diag_rx_base=0U;
    UartStats snapshot;
    /* 仅隔离夹具：先保存本次启动前累计的复位标志，再用HAL位带宏清标志。
       不启动LSI/IWDG、不修改OB/BKP；下一次启动报告才可区分新复位来源。
       第一次下载后的报告可能包含旧累计标志，不能单独判为本次根因。 */
    __HAL_RCC_CLEAR_RESET_FLAGS();
    (void)Uart_Start();
    say("P1 UART TEST ONLY 115200 8N1; e=echo, d=diag, v=RX+CRC/idle5s, x=TX511, h=hold500ms, t=TX256/TC, s=stats, p=pin10s\r\n");
    say("P1 RESET CSR="); number(reset_csr); say("\r\n");
    for (;;) {
        Uart_Service();
        errors=Uart_TakeErrors();
        if (diagnostic) { diag_errors |= errors; }
        if (errors) { report(errors); }
        if (echo) {
            count=0U;
            while (count<sizeof(block) && Uart_ReadByte(&block[count])) { count++; }
            if (count) {
                /* 仅测试夹具前台算CRC，不改ISR和正式UART owner的时序/所有权。 */
                if (diagnostic) {
                    diag_crc=CRC32_Update(diag_crc,block,count);
                    diag_forward+=count;
                    diag_last=HAL_GetTick();
                }
                if (receive_only) { /* 方向隔离：消费数据但不回显，5s后才发送冻结结果。 */ }
                else if (send(block,count)) {
                    if (diagnostic) { diag_tx+=count; }
                } else {
                    errors=Uart_TakeErrors();
                    diag_errors |= errors;
                    report(errors);
                }
            }
            if (diagnostic && (uint32_t)(HAL_GetTick()-diag_last)>=5000U) {
                /* 无新输入5s后一次冻结快照：诊断输出自身不计入payload TX。
                   所有字节（含s/e/d）在echo期间均为数据，不借特殊字节退出。 */
                Uart_GetStats(&snapshot);
                say(receive_only ? "\r\nP1 RX DIAG RXDELTA=" : "\r\nP1 DIAG RXDELTA="); number(snapshot.received_bytes-diag_rx_base);
                say(" FWD="); number(diag_forward);
                say(" CRC="); number(diag_crc^0xFFFFFFFFU);
                say(" TXOK="); number(diag_tx);
                say(" ERR="); number(diag_errors);
                say(" TOTAL="); number(snapshot.total_errors);
                say(" DROP="); number(snapshot.discarded_bytes);
                say(" RECOVER="); number(snapshot.recoveries);
                say(" QUEUE="); number(snapshot.queued_bytes);
                say("\r\nP1 DIAG END\r\n");
                echo=diagnostic=receive_only=0;
            }
        } else if (Uart_ReadByte(&byte)) {
            if (byte=='p') { pin_probe(); }
            else if (byte=='e') { echo=1; say("P1 ECHO READY\r\n"); }
            else if (byte=='d' || byte=='v') {
                Uart_GetStats(&snapshot);
                /* v允许命令+数据同次提交；已入队的payload也属于本会话。
                   uint32自然回绕，不把消费者尚未取出的数据扣成新会话丢数。 */
                receive_only=(byte=='v');
                diag_rx_base=snapshot.received_bytes-(receive_only ? snapshot.queued_bytes : 0U);
                diag_crc=0xFFFFFFFFU;
                diag_forward=diag_tx=diag_errors=0U;
                diag_last=HAL_GetTick();
                echo=diagnostic=1;
                if (!receive_only) { say("P1 DIAG READY\r\n"); }
                /* v开始不发横幅，避免接收payload期间给转接器增加反向负载。 */
            }
            else if (byte=='x') {
                int ok=1;
                /* 511B为同一失败轮纯测试向量；分块仅服从owner的256B上限，
                   连续提交且每块等硬件TC，不插人为延时/降速。 */
                say("P1 TX511 BEGIN\r\n");
                for (count=0U;count<sizeof(phase1_tx_vector);count+=sizeof(block)) {
                    uint16_t n=(uint16_t)(sizeof(phase1_tx_vector)-count);
                    if (n>sizeof(block)) { n=sizeof(block); }
                    if (!send(&phase1_tx_vector[count],n)) { ok=0; break; }
                }
                if (ok) { say("\r\nP1 TX511 END\r\n"); }
                else { report(Uart_TakeErrors()); }
            }
            else if (byte=='s') { report(0U); }
            else if (byte=='h') {
                say("P1 HOLD500 SEND >=4096 BYTES NOW\r\n");
                /* 测试夹具故意暂停消费者，ISR仍接收；保留错误事件供恢复后报告。 */
                until=HAL_GetTick();
                while ((uint32_t)(HAL_GetTick()-until)<500U) { Uart_Service(); }
                errors=Uart_TakeErrors();
                /* 无错误的短输入也不进入后续控制解析：测试边界明确丢弃。 */
                while (Uart_ReadByte(&byte)) { }
                report(errors);
                say("P1 READY\r\n");
            } else if (byte=='t') {
                for (count=0U;count<sizeof(block);count++) { block[count]=(uint8_t)count; }
                if (send(block,sizeof(block)) && send(block,sizeof(block))) { say("P1 TC OK\r\n"); }
                else { report(Uart_TakeErrors()); }
            }
        }
    }
}

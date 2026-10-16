/* 实际夹具的隔离驱动：注入RX缺字节/TX线上缺字节；不接触硬件。 */
#include "uart_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static jmp_buf done;
static UartStats stats;
static unsigned pos, mode, active, emitted;
static uint32_t tick;
static uint8_t output[8192];
static unsigned output_size;
void Phase1_UartMain(void);
uint32_t HAL_GetTick(void) { return tick; }
int Uart_Start(void) { stats.received_bytes=1; return 1; }
void Uart_Service(void) { if(active && pos>=511) tick+=1000; }
uint32_t Uart_TakeErrors(void) { return 0; }
int Uart_ReadByte(uint8_t *b) {
    if(!active) { if(!emitted) { emitted=1; *b='d'; return 1; } return 0; }
    if(mode==1 && pos==58) pos++;
    if(pos>=511) return 0;
    *b=(uint8_t)(pos++); stats.received_bytes++; return 1;
}
void Uart_GetStats(UartStats *s) { *s=stats; }
int Uart_TxSubmit(const uint8_t *data,uint16_t n) {
    unsigned i;
    if(active && pos<=511 && n<=128 && stats.transmitted_bytes<10000) {
        /* 仅payload起始位置由发送流累计识别；诊断时tick已>=5000。 */
        for(i=0;i<n;i++) {
            if(!(mode==2 && tick<5000 && emitted==59)) output[output_size++]=data[i];
            if(tick<5000) emitted++;
        }
    } else { memcpy(output+output_size,data,n); output_size+=n; }
    stats.transmitted_bytes+=n;
    if(n==15 && memcmp(data,"P1 DIAG READY\r\n",15)==0) { active=1; emitted=1; }
    if(n==15 && memcmp(data,"\r\nP1 DIAG END\r\n",15)==0) longjmp(done,1);
    return 1;
}
int Uart_FlushTC(uint32_t ms) { (void)ms; return 1; }
int main(int argc,char **argv) {
#ifdef _WIN32
    _setmode(1,_O_BINARY);
#endif
    if(argc!=2) return 2;
    mode=(unsigned)atoi(argv[1]);
    if(!setjmp(done)) Phase1_UartMain();
    if(fwrite(output,1,output_size,stdout)!=output_size) return 3;
    return 0;
}

/* 实际夹具的隔离驱动：注入RX缺字节/TX线上缺字节；不接触硬件。 */
#include "uart_port.h"
#include "stm32f1xx_hal.h"
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
FixtureGPIO fixture_gpioa={0x444448B4U,GPIO_PIN_10};
FixtureUSART fixture_usart1={0x202CU,1U};
FixtureRCC fixture_rcc={0x08000003U};
static uint8_t output[8192];
static unsigned output_size;
void Phase1_UartMain(void);
uint32_t HAL_GetTick(void) { return tick; }
int Uart_Start(void) { if(fixture_rcc.CSR!=3U) abort(); stats.received_bytes=(mode==6 ? 128U : 1U); stats.queued_bytes=(mode==6 ? 127U : 0U); return 1; }
void Uart_Service(void) {
    if(mode>=7) {
        tick++;
        fixture_gpioa.IDR=(mode>=8 && tick>=2000 && tick<4000) ? 0U : GPIO_PIN_10;
        if(mode==8 && tick==2000) { stats.total_errors++; stats.discarded_bytes++; }
        if(mode==8 && tick==2005) stats.recoveries++;
    } else if(active && pos>=511) tick+=1000;
}
uint32_t Uart_TakeErrors(void) { return mode==8 && tick==2000 ? UART_ERR_FE : 0; }
int Uart_ReadByte(uint8_t *b) {
    if(!active) { if(!emitted) { emitted=1; *b=(mode>=7 ? 'p' : mode==4 ? 'x' : mode>=3 ? 'v' : 'd'); if(mode>=3 && mode<7 && mode!=4) active=1; return 1; } return 0; }
    if((mode==1 || mode==5) && pos==58) pos++;
    if(pos>=511) return 0;
    *b=(uint8_t)(pos++); if(mode==6 && pos<=127) stats.queued_bytes--; else stats.received_bytes++; return 1;
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
    if(n==14 && memcmp(data,"\r\nP1 PIN END\r\n",14)==0) longjmp(done,1);
    if(n==15 && memcmp(data,"P1 DIAG READY\r\n",15)==0) { active=1; emitted=1; }
    if((n==15 && memcmp(data,"\r\nP1 DIAG END\r\n",15)==0) || (n==16 && memcmp(data,"\r\nP1 TX511 END\r\n",16)==0)) longjmp(done,1);
    return 1;
}
int Uart_FlushTC(uint32_t ms) { (void)ms; return 1; }
int main(int argc,char **argv) {
#ifdef _WIN32
    _setmode(1,_O_BINARY);
#endif
    if(argc!=2) return 2;
    mode=(unsigned)atoi(argv[1]);
    if(mode==9) fixture_rcc.CSR=0x6C000003U; /* 多原因位不能被诊断强行互斥化。 */
    if(!setjmp(done)) Phase1_UartMain();
    if(fwrite(output,1,output_size,stdout)!=output_size) return 3;
    return 0;
}

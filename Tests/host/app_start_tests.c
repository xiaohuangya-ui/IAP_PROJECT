/* 运行实际启动夹具，替换HAL和Jump终端；真正Jump准入由既有独立C负例及AC5审查。 */
#include "stm32f1xx_hal.h"
#include "jump.h"
#include "recovery.h"
#include "phase3_app_identity.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
TestFlash test_flash={4U};TestGPIO test_gpio={8U<<16,1U<<12,1U<<12};
int test_tc=1,huart1;
static jmp_buf done;
static unsigned mode, jumped, disabled, ticks;
void Boot_EarlyInit(void);void BootMain(void);
void __disable_irq(void) { disabled=1U; }
void __WFI(void) { longjmp(done,1); }
uint32_t HAL_GetTick(void) { return ticks++; }
uint32_t HAL_RCC_GetHCLKFreq(void) { return mode==2U?8000000U:72000000U; }
int HAL_UART_Transmit(void *u,uint8_t *p,uint16_t n,uint32_t ms)
{(void)u;(void)p;(void)n;(void)ms;return mode==4U?1:0;}
int Recovery_Sample(RecoverySample *s) {(void)s;return mode==1U;}
int SpiBus_Init(void) {return mode!=3U;}
int Jump_Prepare(const ImageIdentity *id,JumpTarget *t)
{
    if(id->size!=P3_APP_SIZE || id->crc32!=P3_APP_CRC32 || id->version!=P3_APP_VERSION) {exit(3);}
    t->identity=*id;t->msp=0x20005000;t->pc=0x08004001;return mode!=5U;
}
__declspec(noreturn) void Jump_Commit(const JumpTarget *t) {(void)t;jumped=1U;longjmp(done,1);}
int main(int argc,char **argv)
{
    if(argc!=2) {return 2;}mode=(unsigned)atoi(argv[1]);
    if(mode==6U){test_flash.OBR=0U;}if(mode==7U){test_gpio.IDR=0U;}
    if(mode==8U){test_tc=0;}
    if(!setjmp(done)){Boot_EarlyInit();BootMain();}
    if(jumped!=(mode==0U) || disabled==(mode==0U)) {return 4;}
    puts("{\"result\":\"PASS\"}");return 0;
}

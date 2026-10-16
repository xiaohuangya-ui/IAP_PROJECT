#include "stm32f1xx_hal.h"
#include "boot_clock.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
ClockRcc rcc;ClockFlash flash_regs;
uint32_t SystemCoreClock=8000000U,uwTickPrio=15U,irq,masked;
static uint32_t tick,calls,init_calls;static int mode;
uint32_t HAL_GetTick(void)
{
    ++calls;assert(calls<9000000U);
    if ((rcc.CR&RCC_CR_HSEON)&&mode!=1&&mode!=5) {rcc.CR|=RCC_CR_HSERDY;}
    if ((rcc.CSR&RCC_CSR_LSION)&&mode!=2) {rcc.CSR|=RCC_CSR_LSIRDY;}
    if (rcc.CR&RCC_CR_PLLON) {
        assert((flash_regs.ACR&7U)==2U);
        assert((rcc.CFGR&RCC_CFGR_PPRE1)==RCC_CFGR_PPRE1_DIV2);
        assert((rcc.CFGR&(RCC_CFGR_PLLSRC|RCC_CFGR_PLLXTPRE|RCC_CFGR_PLLMULL))==0x1D0000U);
        if(mode!=3) {rcc.CR|=RCC_CR_PLLRDY;}
    }
    if ((rcc.CFGR&RCC_CFGR_SW)==2U) {
        assert(rcc.CR&RCC_CR_PLLRDY);
        if(mode!=4) {rcc.CFGR=(rcc.CFGR&~12U)|8U;}
    }
    if(mode!=5&&mode!=6) {++tick;}
    return tick;
}
int HAL_InitTick(uint32_t priority)
{
    ++init_calls;assert(priority==15U);assert(SystemCoreClock==72000000U);
    assert((rcc.CFGR&12U)==8U);return mode==7?1:0;
}
void Error_Handler(void){assert(0);}
int main(int argc,char **argv)
{
    int ok;assert(argc==2);mode=atoi(argv[1]);rcc.CR=3U;
    rcc.CSR=0xA0000000U;rcc.BDCR=0x12345678U;
    if(mode==8) {irq=1;}if(mode==9) {masked=1;}
    if(mode==10) {rcc.CR|=RCC_CR_PLLON;}
    if(mode==11) {rcc.CR|=RCC_CR_HSEBYP;}
    if(mode==12) {SystemCoreClock=72000000U;}
    if(mode==13) {tick=0xFFFFFFF0U;}
    if(mode==14) {rcc.CFGR=8U;}
    ok=BootClock_Init();assert(ok==(mode==0||mode==13));
    assert(rcc.BDCR==0x12345678U);assert((rcc.CSR&0xA0000000U)==0xA0000000U);
    if(mode>=8&&mode<=12) {assert(calls==0);}
    if(mode>=1&&mode<=6) {assert(init_calls==0);assert(SystemCoreClock==8000000U);}
    printf("{\"mode\":%d,\"PASS\":true,\"polls\":%u}\n",mode,calls);return 0;
}

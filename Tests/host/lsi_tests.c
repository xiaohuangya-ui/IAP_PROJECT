#include "stm32f1xx_hal.h"
#include "lsi_measure.h"
#include "boot_iwdg.h"
#include "watchdog_handoff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
RccMock rcc;PwrMock pwr;RtcMock rtc;FlashMock flash_regs;IwdgMock iwdg;
uint32_t irq,clock_hz=72000000,tick;static unsigned mode,checks;
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL mode %u line %d\n",mode,__LINE__);exit(1);}}while(0)
uint32_t HAL_RCC_GetHCLKFreq(void){return clock_hz;}
uint32_t HAL_GetTick(void)
{
    uint32_t count,div;tick++;
    if(mode!=4&&(rcc.CSR&RCC_CSR_LSION)){rcc.CSR|=RCC_CSR_LSIRDY;}
    if(mode!=3){rtc.CRL|=RTC_CRL_RSF;}if(mode!=5){rtc.CRL|=RTC_CRL_RTOFF;}
    count=tick*40U;div=32767U-count%32768U;rtc.CNTH=(count/32768U)>>16;rtc.CNTL=(count/32768U)&65535U;
    rtc.DIVH=div>>16;rtc.DIVL=div&65535U;return tick;
}
int main(int argc,char **argv)
{
    LsiMeasurement m;uint32_t before;int result;
    C(argc==2);mode=(unsigned)atoi(argv[1]);
    rcc.CR=RCC_CR_HSERDY;rcc.CFGR=RCC_CFGR_PLLSRC|RCC_CFGR_SWS_PLL;
    rcc.APB1ENR=0x42;rtc.PRLL=32767;rtc.DIVL=32767;flash_regs.OBR=FLASH_OBR_IWDG_SW;
    if(mode==1){rcc.BDCR=RCC_BDCR_RTCSEL_1|RCC_BDCR_RTCEN;pwr.CR=PWR_CR_DBP;}
    if(mode==2){rcc.BDCR=1U<<8;}if(mode==6){irq=1;}if(mode==7){clock_hz=8000000;}
    before=rcc.BDCR;
    if(mode<8){
        result=Lsi_Measure(&m);
        if(mode<2){C(result);C(m.hz>=39900&&m.hz<=40100);C(m.source_selected==(mode==0));}
        else{C(!result);}
        C((rcc.BDCR&(1U<<16))==0);C((rcc.BDCR&RCC_BDCR_RTCEN)==(before&RCC_BDCR_RTCEN));
        C(rcc.APB1ENR==0x42);C((pwr.CR&PWR_CR_DBP)==(mode==1?PWR_CR_DBP:0));C(rtc.PRLL==32767);
    }else{
        /* 已启动后等待配置同步，再刷新；KR写入完整顺序由独立启动回归检查。 */
        C(Iwdg_StartMeasured());C(iwdg.KR==0xAAAA);C(iwdg.PR==4);C(iwdg.RLR>=2492&&iwdg.RLR<=2505);
        C(!Iwdg_StartMeasured());C(Iwdg_Running());iwdg.KR=0;Iwdg_Progress();C(iwdg.KR==0xAAAA);
        tick+=2001;iwdg.KR=0;Iwdg_Progress();C(iwdg.KR==0&&!Iwdg_Running());
        tick++;Iwdg_Progress();C(iwdg.KR==0);C(!Iwdg_StartMeasured());
        {WatchdogHealth health;uint32_t pr=iwdg.PR,rlr=iwdg.RLR,bdcr=rcc.BDCR;
         AppWatchdog_Init(&health,100,0);C(AppWatchdog_Early(&health));C(!AppWatchdog_Early(&health));
         iwdg.KR=0;C(!AppWatchdog_Healthy(&health,200,0,1,0));C(iwdg.KR==0);
         C(AppWatchdog_Healthy(&health,600,1,1,0));C(iwdg.KR==0xAAAA);
         C(iwdg.PR==pr&&iwdg.RLR==rlr&&rcc.BDCR==bdcr);}
    }
    printf("{\"mode\":%u,\"checks\":%u,\"result\":\"PASS\",\"scope\":\"actual RTC/IWDG owners with register model; no hardware\"}\n",mode,checks);return 0;
}

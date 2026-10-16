#include "lsi_measure.h"
#include "stm32f1xx_hal.h"
#include <string.h>
static int wait_bits(volatile uint32_t *reg,uint32_t mask,uint32_t wanted)
{
    uint32_t start=HAL_GetTick(),count=0;
    while((*reg&mask)!=wanted){
        if((uint32_t)(HAL_GetTick()-start)>=100U||++count>=7200000U){return 0;}
    }return 1;
}
static int sample(uint32_t prl,uint32_t *value)
{
    uint32_t h,l,h2,d,dl,h3,i;
    for(i=0;i<8U;++i){
        h=RTC->CNTH;l=RTC->CNTL;h2=RTC->CNTH;
        d=RTC->DIVH;dl=RTC->DIVL;
        if(h!=h2||d!=RTC->DIVH){continue;}
        h3=RTC->CNTH;
        if(h3!=h||l!=RTC->CNTL){continue;}
        d=((d&15U)<<16)|(dl&65535U);if(d>prl){continue;}
        *value=(((h&65535U)<<16)|(l&65535U))*(prl+1U)+(prl-d);return 1;
    }return 0;
}
int Lsi_Measure(LsiMeasurement *out)
{
    uint32_t clocks,dbp,rtc_en,start,a,b,loops=0;int ok=0;
    if(!out||__get_IPSR()||__get_PRIMASK()||__get_BASEPRI()||__get_FAULTMASK()||
       HAL_RCC_GetHCLKFreq()!=72000000U||(RCC->CR&RCC_CR_HSERDY)==0U||
       (RCC->CFGR&RCC_CFGR_PLLSRC)==0U||(RCC->CFGR&RCC_CFGR_SWS)!=RCC_CFGR_SWS_PLL){return 0;}
    memset(out,0,sizeof(*out));out->bdcr_before=RCC->BDCR;out->csr_before=RCC->CSR;
    if((RCC->BDCR&RCC_BDCR_RTCSEL)!=0U&&(RCC->BDCR&RCC_BDCR_RTCSEL)!=RCC_BDCR_RTCSEL_1){return 0;}
    clocks=RCC->APB1ENR&(RCC_APB1ENR_PWREN|RCC_APB1ENR_BKPEN);
    RCC->APB1ENR|=RCC_APB1ENR_PWREN|RCC_APB1ENR_BKPEN;(void)RCC->APB1ENR;
    dbp=PWR->CR&PWR_CR_DBP;rtc_en=RCC->BDCR&RCC_BDCR_RTCEN;
    PWR->CR|=PWR_CR_DBP;
    if(!wait_bits(&PWR->CR,PWR_CR_DBP,PWR_CR_DBP)){goto done;}
    RCC->CSR|=RCC_CSR_LSION;
    if(!wait_bits(&RCC->CSR,RCC_CSR_LSIRDY,RCC_CSR_LSIRDY)){goto done;}
    if((RCC->BDCR&RCC_BDCR_RTCSEL)==0U){RCC->BDCR|=RCC_BDCR_RTCSEL_1;out->source_selected=1;}
    RCC->BDCR|=RCC_BDCR_RTCEN;
    /* W0C：保留OWF/ALRF/SECF，CNF保持0；不使用会误清并发标志的RMW。 */
    if(RTC->CRL&RTC_CRL_CNF){goto done;}
    RTC->CRL=RTC_CRL_OWF|RTC_CRL_ALRF|RTC_CRL_SECF;
    if(!wait_bits(&RTC->CRL,RTC_CRL_RSF|RTC_CRL_RTOFF,RTC_CRL_RSF|RTC_CRL_RTOFF)){goto done;}
    out->prl=((RTC->PRLH&15U)<<16)|(RTC->PRLL&65535U);
    if(!sample(out->prl,&a)){goto done;}start=HAL_GetTick();
    while((uint32_t)(HAL_GetTick()-start)<500U){if(++loops>=36000000U){goto done;}}
    if(!sample(out->prl,&b)){goto done;}
    out->elapsed_ms=HAL_GetTick()-start;out->counter_before=a;out->counter_after=b;
    if((uint32_t)(out->elapsed_ms-500U)>10U||(uint32_t)(b-a)>40000U){goto done;}
    out->hz=((uint32_t)(b-a)*1000U+out->elapsed_ms/2U)/out->elapsed_ms;
    ok=(uint32_t)(out->hz-30000U)<=30000U;
done:
    RCC->BDCR=(RCC->BDCR&~RCC_BDCR_RTCEN)|rtc_en;
    PWR->CR=(PWR->CR&~PWR_CR_DBP)|dbp;
    if((PWR->CR&PWR_CR_DBP)!=dbp){ok=0;}
    RCC->APB1ENR=(RCC->APB1ENR&~(RCC_APB1ENR_PWREN|RCC_APB1ENR_BKPEN))|clocks;
    return ok;
}

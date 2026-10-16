#include "backup_domain.h"
#include "stm32f1xx.h"
#include <stdio.h>
#include <stdlib.h>
RCC_TypeDef host_rcc;
PWR_TypeDef host_pwr;
BKP_TypeDef host_bkp;
uint32_t host_ipsr,host_primask,host_basepri,host_faultmask,host_control;
static unsigned assertions;
#define CHECK(x) do { ++assertions; if (!(x)) { fprintf(stderr,"FAIL line %u\n",(unsigned)__LINE__); exit(1); } } while(0)
int main(void)
{
    unsigned i,j;
    uint32_t clocks,pwr;
    for(i=0;i<4;++i) for(j=0;j<2;++j) {
        clocks=0x1234U|((i&1)?RCC_APB1ENR_PWREN:0)|((i&2)?RCC_APB1ENR_BKPEN:0);
        pwr=0x42U|(j?PWR_CR_DBP:0);
        RCC->APB1ENR=clocks;RCC->BDCR=0x8200;RCC->CSR=0xC4000003;
        PWR->CR=pwr;BKP->DR1=0;BKP->DR2=0;BKP->DR3=0xA33A;
        CHECK(Backup_AppReadTrial()==0xA33A);
        CHECK(Backup_AppRequest(1)==1 && BKP->DR1==0x55AA);
        CHECK(Backup_AppRequest(2)==1 && BKP->DR2==0x5AA5);
        CHECK(Backup_AppRequest(0)==0 && Backup_AppRequest(3)==0 && Backup_AppRequest(4)==0);
        CHECK(BKP->DR3==0xA33A && RCC->BDCR==0x8200 && RCC->CSR==0xC4000003);
        CHECK(RCC->APB1ENR==clocks && PWR->CR==pwr);
    }
    host_control=2;CHECK(Backup_AppRequest(1)==0);host_control=0;
    host_basepri=0x50;CHECK(Backup_AppRequest(2)==0);host_basepri=0;
    host_faultmask=1;CHECK(Backup_AppRequest(1)==0);host_faultmask=0;
    host_ipsr=1;CHECK(Backup_AppRequest(2)==0);
    printf("{\"result\":\"PASS\",\"assertions\":%u}\n",assertions);
    return 0;
}

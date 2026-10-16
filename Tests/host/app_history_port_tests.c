#include "stm32f1xx_hal.h"
#include "app_history_port.h"
#include "spi_bus.h"
#include "w25q64.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
HGPIO h_gpio;HSPI h_spi;HRCC h_rcc;HAFIO h_afio;uint32_t SystemCoreClock=72000000U;
static uint32_t now,mask,last_address,last_size,calls,frozen;
uint32_t HAL_GetTick(void){return frozen?now:now++;}
uint32_t __get_IPSR(void){return mask&1U;}
uint32_t __get_PRIMASK(void){return mask&2U;}
uint32_t __get_BASEPRI(void){return mask&4U;}
void vTaskDelay(uint32_t n){assert(!mask && n==1);now+=n;}
int W25_Init(const W25Port *p){assert(p && p->clock_ms && p->service && p->progress);p->service();p->progress();return 1;}
int W25_Read(uint32_t a,uint8_t *p,uint32_t n){assert(p);last_address=a;last_size=n;++calls;return 1;}
int W25_Write(uint32_t a,const uint8_t *p,uint32_t n){assert(p);last_address=a;last_size=n;++calls;return 1;}
int W25_Erase(uint32_t a,uint32_t n){last_address=a;last_size=n;++calls;return 1;}
int main(void)
{
    const AppHistoryPort *p=AppHistory_DevicePort();uint8_t b[4096]={0},rx[260];uint32_t o,n,before;
    h_gpio.CRL=0x12345678;h_gpio.CRH=0x87654321;h_rcc.APB2RSTR=0x80000000;h_rcc.APB2ENR=0x80000000;
    assert(p->start());assert(h_gpio.CRL==0xB4B25678 && h_gpio.CRH==0x87654321);
    assert(h_gpio.BSRR==GPIO_PIN_4 && h_rcc.APB2RSTR==0x80000000 && (h_rcc.APB2ENR&0x80000000));
    for(mask=1;mask<8;mask++){assert(!SpiBus_Init());}mask=0;
    SystemCoreClock=8000000;assert(!SpiBus_Init());SystemCoreClock=72000000;
    h_rcc.CFGR=RCC_CFGR_PPRE2;assert(!SpiBus_Init());h_rcc.CFGR=0;
    h_afio.MAPR=AFIO_MAPR_SPI1_REMAP;assert(!SpiBus_Init());h_afio.MAPR=0;
    assert(SpiBus_Init());frozen=1;h_spi.SR=SPI_SR_TXE|SPI_SR_RXNE;
    assert(SpiBus_Transfer(b,rx,260));assert(h_gpio.BSRR==GPIO_PIN_4);
    assert(!SpiBus_Transfer(b,rx,261));assert(!SpiBus_Transfer(NULL,rx,1));assert(!SpiBus_Exchange(b,rx,5));
    for(n=0;n<3;n++){
        assert(SpiBus_Init());h_spi.SR=n==0?0:n==1?SPI_SR_OVR:SPI_SR_BSY;
        assert(!SpiBus_Transfer(b,rx,1));assert(!SpiBus_Transfer(b,rx,1));assert(h_gpio.BSRR==GPIO_PIN_4);
    }
    /* Tick stopped still exits by finite spins; IO timeout locks until reinit. */
    frozen=0;assert(SpiBus_Init());h_spi.SR=SPI_SR_TXE;assert(!SpiBus_Transfer(b,rx,1));
    for(o=0;o<APP_HISTORY_BYTES;o+=4096){
        for(n=1;n<=4096;n*=2){assert(p->read(o,b,n));assert(last_address==APP_HISTORY_BASE+o && last_size==n);}
        assert(p->erase_sector(o));assert(last_address==APP_HISTORY_BASE+o && last_size==4096);
    }
    before=calls;
    assert(!p->read(APP_HISTORY_BYTES,b,1));assert(!p->read(0,b,0));assert(!p->read(0,NULL,1));
    assert(!p->program(0xFFFFFFFFU,b,2));assert(!p->program(APP_HISTORY_BYTES-1,b,2));assert(!p->erase_sector(1));
    assert(!p->erase_sector(APP_HISTORY_BYTES));assert(calls==before);
    assert(p->program(APP_HISTORY_BYTES-1,b,1));assert(last_address==APP_HISTORY_BASE+APP_HISTORY_BYTES-1);
    puts("PASS: actual App port GPIO/clock/context/SPI timeout/frozen tick, full business range/overflow and no IAP access");return 0;
}

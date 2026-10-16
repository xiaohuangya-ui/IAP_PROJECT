#include "jump.h"
#include "spi.h"
#include "usart.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
SPI_TypeDef host_spi;
GPIO_TypeDef host_gpio;
RCC_TypeDef host_rcc;
PWR_TypeDef host_pwr;
BKP_TypeDef host_bkp;
USART_TypeDef host_usart;
DMA_Channel_TypeDef host_dma4,host_dma5;
NVIC_TypeDef host_nvic;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;
uint32_t host_ipsr,host_primask,host_basepri,host_faultmask,host_control;
static int spi_ok=1,w25_ok=1;
static uint8_t status1;
static unsigned assertions, status_reads;
#define CHECK(x) do { ++assertions; if (!(x)) { fprintf(stderr,"FAIL line %u\n",(unsigned)__LINE__); exit(1); } } while(0)
int SpiBus_Idle(void) { return spi_ok; }
int W25_ReadStatus(uint8_t *s1,uint8_t *s2)
{
    ++status_reads;*s1=status1;*s2=0;return w25_ok;
}
static void setup(void)
{
    host_ipsr=host_primask=host_basepri=host_faultmask=host_control=0;
    NVIC->IABR[0]=NVIC->IABR[1]=0;
    DMA1_Channel4->CCR=DMA1_Channel5->CCR=0;
    USART1->SR=USART_SR_TC;RCC->CR=RCC_CR_HSIRDY;
    huart1.gState=huart1.RxState=HAL_UART_STATE_READY;
    spi_ok=w25_ok=1;status1=0;
}
int main(void)
{
    uint8_t *region,*data=(uint8_t *)APP_BASE;
    uint32_t sp=0x20005000,pc=APP_BASE+1,i,before;
    JumpTarget target;
    ImageIdentity id={1,256,0};
    region=VirtualAlloc((void *)0x08000000U,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(region==(uint8_t *)0x08000000U);
    memset(data,0xA5,256);memcpy(data,&sp,4);memcpy(data+4,&pc,4);
    id.crc32=CRC32_IEEE(data,id.size);
    setup();CHECK(Jump_Prepare(&id,&target)==1);
    CHECK(target.msp==sp && target.pc==pc && target.identity.crc32==id.crc32);
    for(i=0;i<16;++i) {
        setup();memset(&target,0xEE,sizeof(target));
        switch(i) {
        case 0:host_ipsr=1;break;case 1:host_primask=1;break;
        case 2:host_basepri=0x50;break;case 3:host_faultmask=1;break;
        case 4:host_control=1;break;case 5:host_control=2;break;
        case 6:NVIC->IABR[0]=1;break;case 7:NVIC->IABR[1]=1;break;
        case 8:USART1->SR=0;break;case 9:huart1.gState=0;break;
        case 10:huart1.RxState=0;break;case 11:DMA1_Channel4->CCR=1;break;
        case 12:DMA1_Channel5->CCR=1;break;case 13:RCC->CR=0;break;
        case 14:spi_ok=0;break;case 15:w25_ok=0;break;
        }
        before=USART1->SR;
        CHECK(Jump_Prepare(&id,&target)==0);
        CHECK(target.msp==0xEEEEEEEEU && target.pc==0xEEEEEEEEU);
        CHECK(USART1->SR==before);
    }
    for(i=1;i<4;++i) { setup();status1=(uint8_t)i;CHECK(Jump_Prepare(&id,&target)==0); }
    setup();before=status_reads;
    data[255]^=1;CHECK(Jump_Prepare(&id,&target)==0);data[255]^=1;
    CHECK(status_reads==before);
    id.version=0;CHECK(Jump_Prepare(&id,&target)==0);id.version=1;
    id.size=0xFFFFFFFFU;CHECK(Jump_Prepare(&id,&target)==0);id.size=256;
    pc=APP_BASE;memcpy(data+4,&pc,4);id.crc32=CRC32_IEEE(data,256);
    CHECK(Jump_Prepare(&id,&target)==0);
    CHECK(Jump_Prepare(NULL,&target)==0);CHECK(Jump_Prepare(&id,NULL)==0);
    CHECK(VirtualFree(region,0,MEM_RELEASE)!=0);
    printf("{\"result\":\"PASS\",\"assertions\":%u}\n",assertions);
    return 0;
}

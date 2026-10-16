#include "spi_bus.h"
#include "w25q64.h"
#include "backup_domain.h"
#include "image_contract.h"
#include "spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
SPI_TypeDef host_spi;
GPIO_TypeDef host_gpio;
RCC_TypeDef host_rcc;
PWR_TypeDef host_pwr;
BKP_TypeDef host_bkp;
SPI_HandleTypeDef hspi1;
uint32_t host_ipsr,host_primask,host_basepri,host_faultmask,host_control;
static uint32_t frequency=72000000;
static unsigned assertions, transfers,cs_low,cs_high;
static int inject;
#define CHECK(x) do { ++assertions; if (!(x)) { fprintf(stderr,"FAIL line %u\n",(unsigned)__LINE__); exit(1); } } while(0)
uint32_t HAL_RCC_GetPCLK2Freq(void) { return frequency; }
void HAL_GPIO_WritePin(GPIO_TypeDef *gpio,uint32_t pin,uint32_t state)
{
    CHECK(gpio==GPIOA && pin==GPIO_PIN_4);
    if (state) { gpio->ODR|=pin; ++cs_high; } else { gpio->ODR&=~pin; ++cs_low; }
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,uint8_t *tx,
                                         uint8_t *rx,uint16_t n,uint32_t timeout)
{
    CHECK(spi==&hspi1 && timeout==50 && (GPIOA->ODR&GPIO_PIN_4)==0);
    CHECK((tx[0]==0x9F && n==4) || ((tx[0]==0x05 || tx[0]==0x35) && n==2));
    ++transfers;
    memset(rx,0,n);
    if (tx[0]==0x9F) { rx[1]=0xEF;rx[2]=0x40;rx[3]=0x17; }
    if (tx[0]==0x35) { rx[1]=0x40; }
    return inject;
}
static void setup(void)
{
    host_spi.CR1=SPI_CR1_MSTR|SPI_CR1_CPOL|SPI_CR1_CPHA|SPI_CR1_BR_0|
                 SPI_CR1_BR_1|SPI_CR1_SSM|SPI_CR1_SSI;
    host_spi.CR2=0;host_spi.SR=SPI_SR_TXE;
    host_gpio.CRL=2U<<16;host_gpio.ODR=GPIO_PIN_4;
    hspi1.State=HAL_SPI_STATE_READY;
    inject=HAL_OK;frequency=72000000;
    host_ipsr=host_primask=host_basepri=host_faultmask=host_control=0;
}
int main(void)
{
    uint32_t jedec,old_clock,old_pwr,old_bdcr,old_csr,i,j,old_transfers;
    uint8_t sr1,sr2,tx[4]={0x9F,0xFF,0xFF,0xFF},rx[4];
    BackupSnapshot snap;
    setup(); CHECK(SpiBus_Init()==1); CHECK((SPI1->CR1&SPI_CR1_SPE)!=0);
    CHECK(W25_ReadJedec(&jedec)==1 && jedec==0xEF4017);
    CHECK(W25_ReadStatus(&sr1,&sr2)==1 && sr1==0 && sr2==0x40);
    CHECK(cs_low==3 && transfers==3 && cs_high==4);
    old_transfers=transfers;
    CHECK(W25_ReadJedec(NULL)==0); CHECK(W25_ReadStatus(NULL,&sr2)==0);
    CHECK(W25_ReadStatus(&sr1,NULL)==0);
    CHECK(SpiBus_Exchange(NULL,rx,4)==0); CHECK(SpiBus_Exchange(tx,NULL,4)==0);
    CHECK(SpiBus_Exchange(tx,rx,0)==0); CHECK(SpiBus_Exchange(tx,rx,5)==0);
    CHECK(transfers==old_transfers);
    for(i=1;i<=3;++i) {
        setup(); CHECK(SpiBus_Init()==1); inject=(int)i;
        CHECK(W25_ReadJedec(&jedec)==0); CHECK((GPIOA->ODR&GPIO_PIN_4)!=0);
        old_transfers=transfers;inject=0;
        CHECK(W25_ReadJedec(&jedec)==0 && transfers==old_transfers);
        setup(); CHECK(SpiBus_Init()==1); CHECK(W25_ReadJedec(&jedec)==1);
    }
    setup();frequency=8000000;CHECK(SpiBus_Init()==0);
    setup();SPI1->CR1&=~SPI_CR1_CPOL;CHECK(SpiBus_Init()==0);
    setup();GPIOA->CRL=0;CHECK(SpiBus_Init()==0);
    setup();SPI1->SR|=SPI_SR_BSY;CHECK(SpiBus_Init()==0);
    SPI1->SR=SPI_SR_TXE;old_transfers=transfers;
    CHECK(W25_ReadJedec(&jedec)==0 && transfers==old_transfers);
    for(i=0;i<4;++i) {
        setup();CHECK(SpiBus_Init()==1);
        if(i==0) { SPI1->SR|=SPI_SR_BSY; }
        if(i==1) { SPI1->SR|=SPI_SR_OVR; }
        if(i==2) { SPI1->CR1&=~SPI_CR1_SPE; }
        if(i==3) { host_ipsr=1; }
        old_transfers=transfers;CHECK(W25_ReadJedec(&jedec)==0 && transfers==old_transfers);
    }
    setup();
    /* 访问前/后时钟、DBP及不相关位必须一致；只验证软件寄存器模型。
     * 模型不模拟APB同步/实际写保护/跨复位保持，均需硬件退出证据。 */
    for(i=0;i<4;++i) for(j=0;j<2;++j) {
        old_clock=0x1234U|((i&1)?RCC_APB1ENR_PWREN:0)|((i&2)?RCC_APB1ENR_BKPEN:0);
        old_pwr=0x42U|(j?PWR_CR_DBP:0);
        RCC->APB1ENR=old_clock;PWR->CR=old_pwr;
        old_bdcr=RCC->BDCR=0x8200;old_csr=RCC->CSR=0xC4000003;
        BKP->DR1=0x55AA;BKP->DR2=0x5AA5;BKP->DR3=0xA33A;
        Backup_ReadSnapshot(&snap);
        CHECK(snap.dr1==0x55AA && snap.dr2==0x5AA5 && snap.dr3==0xA33A);
        CHECK(RCC->APB1ENR==old_clock && PWR->CR==old_pwr);
        CHECK(Backup_BootWrite(1,0)==1 && BKP->DR1==0);
        CHECK(Backup_BootWrite(2,0)==1 && BKP->DR2==0);
        CHECK(Backup_BootWrite(3,0)==1 && BKP->DR3==0);
        CHECK(Backup_BootWrite(3,BKP_TRIAL_MARK)==1 && BKP->DR3==BKP_TRIAL_MARK);
        CHECK(RCC->APB1ENR==old_clock && PWR->CR==old_pwr && RCC->BDCR==old_bdcr && RCC->CSR==old_csr);
        CHECK(Backup_BootWrite(0,0)==0 && Backup_BootWrite(4,0)==0);
        CHECK(Backup_BootWrite(1,BKP_UPDATE_REQ)==0 && Backup_BootWrite(2,BKP_CONFIRM_REQ)==0);
        CHECK(Backup_BootWrite(3,1)==0);
    }
    host_primask=1;CHECK(Backup_BootWrite(3,0)==0);host_primask=0;
    host_ipsr=1;CHECK(Backup_BootWrite(3,0)==0);
    printf("{\"result\":\"PASS\",\"assertions\":%u}\n",assertions);
    return 0;
}

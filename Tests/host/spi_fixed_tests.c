/* 实际spi_bus C寄存器模型：字节顺序/连续CS/错误/停Tick/句柄生命周期。
 * 不模拟电气或真实时限；旧HAL入口只用于before差分，产品不链接模型。 */
#include "spi_bus.h"
#include "spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
SPI_TypeDef host_spi;GPIO_TypeDef host_gpio;SPI_HandleTypeDef hspi1;
uint32_t host_ipsr,host_primask,host_basepri,host_faultmask,host_control;
static unsigned checks,writes,reads,cs_low,cs_high,mode,polls;
static uint32_t tick;static uint8_t stream[260];
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d mode=%u\n",__LINE__,mode);exit(1);}}while(0)
uint32_t HAL_RCC_GetPCLK2Freq(void){return 72000000U;}
uint32_t HAL_GetTick(void){if(mode==6){tick+=25U;}else if(mode!=4&&(mode!=0||(polls&63U)==0U)){++tick;}return tick;}
void HAL_GPIO_WritePin(GPIO_TypeDef *g,uint32_t p,uint32_t s)
{C(g==GPIOA&&p==GPIO_PIN_4);if(s){host_gpio.ODR|=p;++cs_high;}else{host_gpio.ODR&=~p;++cs_low;}}
uint32_t SpiBus_TestStatus(void)
{
    ++polls;
    if(writes && mode==1){return SPI_SR_TXE|SPI_SR_BSY;} /* RXNE永不到 */
    if(writes && mode==2){return SPI_SR_RXNE|SPI_SR_TXE|SPI_SR_OVR;}
    if(writes && mode==3){return SPI_SR_RXNE|SPI_SR_TXE|SPI_SR_MODF;}
    if(writes && mode==4){return SPI_SR_BSY;} /* 时基停止且硬件停止 */
    if(writes && reads==writes && mode==5){return SPI_SR_BSY|SPI_SR_TXE;}
    return host_spi.SR;
}
void SpiBus_TestWrite(uint8_t v)
{
    C(!(host_gpio.ODR&GPIO_PIN_4));C(writes<260);C(writes==reads);
    C(hspi1.State==HAL_SPI_STATE_BUSY_TX_RX&&hspi1.Lock==HAL_LOCKED);
    stream[writes++]=v;host_spi.DR=(uint8_t)(v^0xA5U);host_spi.SR=SPI_SR_TXE|SPI_SR_RXNE;
}
uint8_t SpiBus_TestRead(void)
{C(reads<writes);++reads;host_spi.SR=SPI_SR_TXE;return (uint8_t)host_spi.DR;}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *s,uint8_t *tx,uint8_t *rx,uint16_t n,uint32_t timeout)
{unsigned i;(void)s;(void)timeout;for(i=0;i<n;++i){rx[i]=(uint8_t)(tx[i]^0xA5);}return HAL_OK;}
static void setup(unsigned m)
{
    memset(&hspi1,0,sizeof(hspi1));memset(&host_spi,0,sizeof(host_spi));
    host_spi.CR1=SPI_CR1_MSTR|SPI_CR1_CPOL|SPI_CR1_CPHA|SPI_CR1_BR_0|SPI_CR1_BR_1|SPI_CR1_SSM|SPI_CR1_SSI;
    host_spi.SR=SPI_SR_TXE;host_gpio.CRL=2U<<16;host_gpio.ODR=GPIO_PIN_4;
    hspi1.Instance=SPI1;hspi1.State=HAL_SPI_STATE_READY;
    mode=m;writes=reads=polls=cs_low=cs_high=0;tick=0;
    host_ipsr=host_primask=host_basepri=host_faultmask=0;
}
int main(void)
{
    uint8_t tx[260],rx[260];unsigned i,n;
    /* 首个before负例：固定MSB-first owner必须拒绝LSB-first配置。 */
    setup(0);host_spi.CR1|=SPI_CR1_LSBFIRST;C(!SpiBus_Init());
    for(i=0;i<260;++i){tx[i]=(uint8_t)(i*31+7);}
    for(n=1;n<=260;++n){
        setup(0);C(SpiBus_Init());tick=0xFFFFFFFEU;C(SpiBus_Transfer(tx,rx,(uint16_t)n));
        C(writes==n&&reads==n&&cs_low==1&&cs_high==2);C(!memcmp(tx,stream,n));
        C(hspi1.State==HAL_SPI_STATE_READY&&hspi1.Lock==HAL_UNLOCKED&&hspi1.ErrorCode==HAL_SPI_ERROR_NONE);
        C(hspi1.TxXferCount==0&&hspi1.RxXferCount==0&&hspi1.TxXferSize==n&&hspi1.RxXferSize==n);
        for(i=0;i<n;++i){C(rx[i]==(uint8_t)(tx[i]^0xA5U));}
    }
    for(i=1;i<=6;++i){
        setup(i);C(SpiBus_Init());C(!SpiBus_Transfer(tx,rx,2));
        C(cs_low==1&&cs_high==2&&(host_gpio.ODR&GPIO_PIN_4));
        C(hspi1.Lock==HAL_UNLOCKED&&hspi1.State==HAL_SPI_STATE_READY&&hspi1.ErrorCode!=0);
        C(!SpiBus_Idle());C(!SpiBus_Transfer(tx,rx,2));C(polls<=3600010U);
    }
    setup(0);C(SpiBus_Init());hspi1.Lock=HAL_LOCKED;C(!SpiBus_Transfer(tx,rx,2));C(cs_low==0);
    setup(0);C(SpiBus_Init());C(!SpiBus_Transfer(0,rx,2));C(!SpiBus_Transfer(tx,0,2));
    C(!SpiBus_Transfer(tx,rx,0));C(!SpiBus_Transfer(tx,rx,261));C(!SpiBus_Exchange(tx,rx,5));C(cs_low==0);
    setup(0);C(SpiBus_Init());host_primask=1;C(!SpiBus_Transfer(tx,rx,2));C(cs_low==0);
    printf("{\"result\":\"PASS\",\"checks\":%u,\"length_cases\":260,\"fault_cases\":6}\n",checks);return 0;
}

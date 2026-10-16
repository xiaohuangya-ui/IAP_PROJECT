/* App运行期SPI1唯一owner。Boot和App互斥运行，复用同一物理W25驱动源码，
 * 不复制驱动；业务端口仅映射授权1MiB，不能访问任何IAP分区。
 * 所有操作在Business任务，UART/UpgradeCtrl/ISR不触SPI。 */
#include "app_history_port.h"
#include "w25q64.h"
#include "spi_bus.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
static uint8_t bus_ready;
static uint32_t start_ms,spins;
static int context_ok(void)
{return !__get_IPSR() && !__get_PRIMASK() && !__get_BASEPRI();}
static int wait_flag(uint32_t mask,uint32_t value)
{
    while((SPI1->SR&mask)!=value){
        if((SPI1->SR&(SPI_SR_OVR|SPI_SR_MODF)) || (uint32_t)(HAL_GetTick()-start_ms)>=10U || ++spins>=720000U){return 0;}
    }
    return !(SPI1->SR&(SPI_SR_OVR|SPI_SR_MODF)) && (uint32_t)(HAL_GetTick()-start_ms)<10U;
}
int SpiBus_Init(void)
{
    bus_ready=0U;
    if(!context_ok() || SystemCoreClock!=72000000U || (RCC->CFGR&RCC_CFGR_PPRE2)){return 0;}
    RCC->APB2ENR|=RCC_APB2ENR_AFIOEN|RCC_APB2ENR_IOPAEN|RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;
    if(AFIO->MAPR&AFIO_MAPR_SPI1_REMAP){return 0;}
    /* 先释放CS，再只改PA4..7，绝不碰USART1/SWD/OLED/Recovery引脚。 */
    GPIOA->BSRR=GPIO_PIN_4;
    GPIOA->CRL=(GPIOA->CRL&0x0000FFFFU)|0xB4B20000U;
    RCC->APB2RSTR|=RCC_APB2RSTR_SPI1RST;RCC->APB2RSTR&=~RCC_APB2RSTR_SPI1RST;
    SPI1->CR2=0U;SPI1->I2SCFGR=0U;
    SPI1->CR1=SPI_CR1_CPOL|SPI_CR1_CPHA|SPI_CR1_MSTR|SPI_CR1_BR_0|SPI_CR1_BR_1|SPI_CR1_SSM|SPI_CR1_SSI|SPI_CR1_SPE;
    if((GPIOA->CRL&0xFFFF0000U)!=0xB4B20000U ||
       SPI1->CR1!=(SPI_CR1_CPOL|SPI_CR1_CPHA|SPI_CR1_MSTR|SPI_CR1_BR_0|SPI_CR1_BR_1|SPI_CR1_SSM|SPI_CR1_SSI|SPI_CR1_SPE)){return 0;}
    bus_ready=1U;return 1;
}
int SpiBus_Transfer(const uint8_t *tx,uint8_t *rx,uint16_t count)
{
    uint32_t i;int ok=0;
    if(!bus_ready || !tx || !rx || !count || count>260U || !context_ok()){return 0;}
    start_ms=HAL_GetTick();spins=0U;
    if(!wait_flag(SPI_SR_BSY,0U)){bus_ready=0U;return 0;}
    GPIOA->BRR=GPIO_PIN_4;
    for(i=0;i<count;++i){
        if(!wait_flag(SPI_SR_TXE,SPI_SR_TXE)){goto done;}
        *(__IO uint8_t *)&SPI1->DR=tx[i];
        if(!wait_flag(SPI_SR_RXNE,SPI_SR_RXNE)){goto done;}
        rx[i]=*(__IO uint8_t *)&SPI1->DR;
    }
    ok=wait_flag(SPI_SR_TXE,SPI_SR_TXE) && wait_flag(SPI_SR_BSY,0U);
done:
    GPIOA->BSRR=GPIO_PIN_4;
    if(!ok){bus_ready=0U;}
    return ok;
}
int SpiBus_Exchange(const uint8_t *tx,uint8_t *rx,uint16_t count)
{return count<=4U && SpiBus_Transfer(tx,rx,count);}
int SpiBus_Idle(void){return bus_ready && !(SPI1->SR&SPI_SR_BSY) && (GPIOA->ODR&GPIO_PIN_4);}
/* 高/中任务仍由FreeRTOS调度；不能从这里调用UART owner或直接喂IWDG。
 * WIP等待每轮让出1ms；GPIO/SPI事务内不让出，最多10ms且另有限迭代。 */
static void service(void){vTaskDelay(pdMS_TO_TICKS(1U));}
static void progress(void){}
static int start(void)
{
    static const W25Port p={HAL_GetTick,service,progress};
    return SpiBus_Init() && W25_Init(&p);
}
static int range(uint32_t offset,uint32_t bytes)
{return bytes && offset<APP_HISTORY_BYTES && bytes<=APP_HISTORY_BYTES-offset;}
static int read_data(uint32_t offset,uint8_t *out,uint32_t bytes)
{return out && range(offset,bytes) && W25_Read(APP_HISTORY_BASE+offset,out,bytes);}
static int program(uint32_t offset,const uint8_t *data,uint32_t bytes)
{return data && range(offset,bytes) && W25_Write(APP_HISTORY_BASE+offset,data,bytes);}
static int erase(uint32_t offset)
{return !(offset&4095U) && range(offset,4096U) && W25_Erase(APP_HISTORY_BASE+offset,4096U);}
const AppHistoryPort *AppHistory_DevicePort(void)
{
    static const AppHistoryPort port={start,read_data,program,erase};return &port;
}

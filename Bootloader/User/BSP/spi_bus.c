#include "control_zi.h"
#include "spi_bus.h"
#include "spi.h"
#include "main.h"
#include "boot_clock.h"
static BOOT_CONTROL_ZI uint8_t faulted;
#ifdef IAP_STORAGE_ENABLED
/* 固定SPI1模式由唯一BSP特化；HAL仍初始化句柄，无新增IRQ或DMA owner。
 * 主机仅替换DR/SR访问以模拟副作用，正式Target不定义此宏。 */
#ifdef SPI_BUS_HOST_TEST
#define BUS_STATUS() SpiBus_TestStatus()
#define BUS_WRITE(v) SpiBus_TestWrite(v)
#define BUS_READ() SpiBus_TestRead()
#else
#define BUS_STATUS() (SPI1->SR)
#define BUS_WRITE(v) (*(volatile uint8_t *)&SPI1->DR=(v))
#define BUS_READ() (*(volatile uint8_t *)&SPI1->DR)
#endif
static int wait_flag(uint32_t mask,uint32_t wanted,uint32_t start,uint32_t *budget)
{
    uint32_t sr;
    for(;;){
        sr=BUS_STATUS();
        if(sr&(SPI_SR_OVR|SPI_SR_MODF)){
            hspi1.ErrorCode=(sr&SPI_SR_OVR)?HAL_SPI_ERROR_OVR:HAL_SPI_ERROR_MODF;return 0;
        }
        if((uint32_t)(HAL_GetTick()-start)>=50U||!*budget){hspi1.ErrorCode=HAL_SPI_ERROR_FLAG;return 0;}
        --*budget;
        if((sr&mask)==wanted){return 1;}
    }
}
static int exchange_fixed(const uint8_t *tx,uint8_t *rx,uint16_t n)
{
    uint32_t start=HAL_GetTick(),budget=3600000U;int ok=0;
    /* 1. 仅前台调用，句柄锁先于CS；同一事务共用50ms/有限轮询预算。 */
    hspi1.Lock=HAL_LOCKED;hspi1.State=HAL_SPI_STATE_BUSY_TX_RX;hspi1.ErrorCode=HAL_SPI_ERROR_NONE;
    hspi1.pTxBuffPtr=tx;hspi1.pRxBuffPtr=rx;
    hspi1.TxXferSize=hspi1.TxXferCount=n;hspi1.RxXferSize=hspi1.RxXferCount=n;
    HAL_GPIO_WritePin(W25_CS_GPIO_Port,W25_CS_Pin,GPIO_PIN_RESET);
    while(hspi1.RxXferCount){
        if(!wait_flag(SPI_SR_TXE,SPI_SR_TXE,start,&budget)){goto done;}
        BUS_WRITE(*hspi1.pTxBuffPtr++);--hspi1.TxXferCount;
        /* 2. 每发一字节立即读一字节，避免RX积压；DR必须8bit访问。 */
        if(!wait_flag(SPI_SR_RXNE,SPI_SR_RXNE,start,&budget)){goto done;}
        *hspi1.pRxBuffPtr++=BUS_READ();--hspi1.RxXferCount;
    }
    ok=wait_flag(SPI_SR_TXE|SPI_SR_BSY,SPI_SR_TXE,start,&budget);
done:
    /* 3. 故障不猜测清OVR/MODF或重启，保留现场并锁错；没有介质成功承诺。
     * 句柄READY只表示软件事务退出，faulted/SR仍决定是否可再用。 */
    HAL_GPIO_WritePin(W25_CS_GPIO_Port,W25_CS_Pin,GPIO_PIN_SET);
    hspi1.State=HAL_SPI_STATE_READY;hspi1.Lock=HAL_UNLOCKED;return ok;
}
#endif
int SpiBus_Init(void)
{
    HAL_GPIO_WritePin(W25_CS_GPIO_Port, W25_CS_Pin, GPIO_PIN_SET);
    if (
#ifdef IAP_PRODUCT_ENABLED
        !BootClock_Pclk2Ready() ||
#else
        HAL_RCC_GetPCLK2Freq() != 72000000U ||
#endif
        hspi1.State != HAL_SPI_STATE_READY ||
        (SPI1->CR1 & (SPI_CR1_MSTR | SPI_CR1_CPOL | SPI_CR1_CPHA |
         SPI_CR1_BR | SPI_CR1_DFF | SPI_CR1_SSM | SPI_CR1_SSI
#ifdef IAP_STORAGE_ENABLED
         | SPI_CR1_LSBFIRST | SPI_CR1_RXONLY | SPI_CR1_CRCEN | SPI_CR1_BIDIMODE
#endif
         )) !=
        (SPI_CR1_MSTR | SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_BR_0 |
         SPI_CR1_BR_1 | SPI_CR1_SSM | SPI_CR1_SSI) ||
        ((GPIOA->CRL >> 16) & 15U) != 2U) { faulted = 1U; return 0; }
    /* 保留P1-D2显式SPE修复；存储Target传输由本owner固定模式特化。 */
    __HAL_SPI_ENABLE(&hspi1);
    faulted = 0U;
    if (!SpiBus_Idle()) { faulted = 1U; return 0; }
    return 1;
}
int SpiBus_Idle(void)
{
    return faulted == 0U && hspi1.State == HAL_SPI_STATE_READY &&
           (GPIOA->ODR & W25_CS_Pin) != 0U &&
           (SPI1->CR1 & SPI_CR1_SPE) != 0U &&
           (SPI1->SR & (SPI_SR_BSY | SPI_SR_RXNE | SPI_SR_OVR | SPI_SR_MODF)) == 0U &&
           (SPI1->SR & SPI_SR_TXE) != 0U && SPI1->CR2 == 0U;
}
int SpiBus_Exchange(const uint8_t *tx, uint8_t *rx, uint16_t count)
{
#ifdef IAP_STORAGE_ENABLED
    return count<=4U && SpiBus_Transfer(tx,rx,count);
}
int SpiBus_Transfer(const uint8_t *tx,uint8_t *rx,uint16_t count)
{
#endif
#ifndef IAP_STORAGE_ENABLED
    HAL_StatusTypeDef status;
#endif
    if (tx == 0 || rx == 0 || count == 0U ||
#ifdef IAP_STORAGE_ENABLED
        count > 260U ||
#else
        count > 4U ||
#endif
        __get_IPSR() != 0U || __get_PRIMASK() != 0U ||
        __get_BASEPRI() != 0U || __get_FAULTMASK() != 0U || !SpiBus_Idle()
#ifdef IAP_STORAGE_ENABLED
        || hspi1.Lock!=HAL_UNLOCKED || hspi1.Instance!=SPI1
#endif
        ) { return 0; }
#ifdef IAP_STORAGE_ENABLED
    if(!exchange_fixed(tx,rx,count)){faulted=1U;return 0;}
#else
    HAL_GPIO_WritePin(W25_CS_GPIO_Port, W25_CS_Pin, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, count, 50U);
    /* HAL全双工逐字节读RX，完成路径等待BSY。失败仍立即释放CS、锁错。 */
    HAL_GPIO_WritePin(W25_CS_GPIO_Port, W25_CS_Pin, GPIO_PIN_SET);
    if (status != HAL_OK) { faulted = 1U; return 0; }
#endif
    if (!SpiBus_Idle()) { faulted = 1U; return 0; }
    return 1;
}

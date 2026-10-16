#ifndef HOST_SPI_H
#define HOST_SPI_H
#include "stm32f1xx.h"
typedef struct { uint32_t State,Lock,ErrorCode; SPI_TypeDef *Instance;
    const uint8_t *pTxBuffPtr;uint8_t *pRxBuffPtr;
    uint16_t TxXferSize,TxXferCount,RxXferSize,RxXferCount;
} SPI_HandleTypeDef;
typedef int HAL_StatusTypeDef;
#define HAL_SPI_STATE_READY 1U
#define HAL_SPI_STATE_BUSY_TX_RX 5U
#define HAL_UNLOCKED 0U
#define HAL_LOCKED 1U
#define HAL_SPI_ERROR_NONE 0U
#define HAL_SPI_ERROR_MODF 1U
#define HAL_SPI_ERROR_OVR 4U
#define HAL_SPI_ERROR_FLAG 32U
#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_BUSY 2
#define HAL_TIMEOUT 3
#define GPIO_PIN_4 (1U<<4)
#define GPIO_PIN_SET 1U
#define GPIO_PIN_RESET 0U
extern SPI_HandleTypeDef hspi1;
uint32_t HAL_RCC_GetPCLK2Freq(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *gpio,uint32_t pin,uint32_t state);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,uint8_t *tx,
                                         uint8_t *rx,uint16_t n,uint32_t timeout);
#define __HAL_SPI_ENABLE(h) ((void)(h), SPI1->CR1 |= SPI_CR1_SPE)
uint32_t HAL_GetTick(void);
#ifdef SPI_BUS_HOST_TEST
uint32_t SpiBus_TestStatus(void);
void SpiBus_TestWrite(uint8_t value);
uint8_t SpiBus_TestRead(void);
#endif
#endif

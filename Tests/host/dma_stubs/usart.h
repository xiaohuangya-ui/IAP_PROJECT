#ifndef UART_STUBS_H
#define UART_STUBS_H
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct {uint16_t counter;} DMA_HandleTypeDef;
typedef struct { uint32_t ErrorCode; DMA_HandleTypeDef *hdmarx;} UART_HandleTypeDef;
#define __HAL_DMA_GET_COUNTER(d) ((d)->counter)
#define __DMB() __sync_synchronize()
#define HAL_UART_ERROR_DMA 16U
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *,uint8_t *,uint16_t);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *,uint16_t);
extern UART_HandleTypeDef huart1;
extern uint32_t mock_primask, mock_ipsr, mock_basepri, mock_faultmask, mock_tc, mock_rxne;
#define RESET 0U
#define UART_FLAG_TC 1U
#define UART_FLAG_RXNE 2U
#define HAL_UART_ERROR_NONE 0U
#define HAL_UART_ERROR_PE 1U
#define HAL_UART_ERROR_NE 2U
#define HAL_UART_ERROR_FE 4U
#define HAL_UART_ERROR_ORE 8U
#define __HAL_UART_GET_FLAG(u,f) ((f)==UART_FLAG_TC ? mock_tc : mock_rxne)
#define __HAL_UART_CLEAR_OREFLAG(u) (mock_rxne=0U)
#define __get_PRIMASK() mock_primask
#define __get_IPSR() mock_ipsr
#define __get_BASEPRI() mock_basepri
#define __get_FAULTMASK() mock_faultmask
void Mock_DisableIRQ(void);
#define __disable_irq() Mock_DisableIRQ()
#define __set_PRIMASK(k) (mock_primask=(k))
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
#endif

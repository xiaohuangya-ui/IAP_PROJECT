#ifndef P9_STUB_I2C
#define P9_STUB_I2C
#include "adc.h"
typedef int I2C_HandleTypeDef;
extern I2C_HandleTypeDef hi2c1;
#define I2C_MEMADD_SIZE_8BIT 1
#define I2C_FLAG_BUSY 1
int mock_busy(void);
#define __HAL_I2C_GET_FLAG(h,f) mock_busy()
#define __HAL_RCC_I2C1_FORCE_RESET() mock_i2c_reset(1)
#define __HAL_RCC_I2C1_RELEASE_RESET() mock_i2c_reset(0)
void mock_i2c_reset(int active);
void MX_I2C1_Init(void);
int HAL_I2C_Init(I2C_HandleTypeDef *h);
int HAL_I2C_DeInit(I2C_HandleTypeDef *h);
int HAL_I2C_Mem_Write(I2C_HandleTypeDef *h,uint16_t addr,uint16_t control,uint16_t mode,uint8_t *data,uint16_t size,uint32_t timeout);
#endif

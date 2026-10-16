/* 固定产品SPI1初始化owner；Phase7/8源组替换生成spi.c，厂商文件不修改。
 * 保留HAL句柄诊断字段，传输/CS/故障仍归SpiBus，不增加第二条传输路径。
 * 仅启动前调用；SPE由SpiBus_Init在完整配置核验后显式开启。 */
#include "spi.h"
SPI_HandleTypeDef hspi1;
void MX_SPI1_Init(void)
{
    if(hspi1.State!=HAL_SPI_STATE_RESET){Error_Handler();return;}
    hspi1.Instance=SPI1;
    hspi1.Init.Mode=SPI_MODE_MASTER;
    hspi1.Init.Direction=SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize=SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity=SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase=SPI_PHASE_2EDGE;
    hspi1.Init.NSS=SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit=SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode=SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation=SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial=10U;
    __HAL_RCC_SPI1_CLK_ENABLE();__HAL_RCC_GPIOA_CLK_ENABLE();
    /* PA5/7 AF推挽50MHz、PA6浮空输入；只改SPI引脚，保持CS与其他配置。 */
    MODIFY_REG(GPIOA->CRL,0xFFF00000U,0xB4B00000U);
    __HAL_SPI_DISABLE(&hspi1);
    SPI1->CR1=SPI_CR1_MSTR|SPI_CR1_SSI|SPI_CR1_SSM|SPI_CR1_CPOL|SPI_CR1_CPHA|SPI_CR1_BR_0|SPI_CR1_BR_1;
    SPI1->CR2=0U;
    CLEAR_BIT(SPI1->I2SCFGR,SPI_I2SCFGR_I2SMOD);
    hspi1.Lock=HAL_UNLOCKED;hspi1.ErrorCode=HAL_SPI_ERROR_NONE;hspi1.State=HAL_SPI_STATE_READY;
}

#include "app_oled_port.h"
#include "i2c.h"
int AppOled_Write(void *context,uint8_t control,const uint8_t *data,uint16_t size)
{
    uint32_t start,spins=0U,elapsed;
    (void)context;
    if(!data || !size || size>32U || (control!=0U && control!=0x40U)){return 0;}
    if(__get_IPSR() || __get_PRIMASK() || __get_BASEPRI()){return 0;}
    /* 1. F1 HAL在置STOP后立即返回，BUSY可能尚未释放；不能把正常
     * STOP尾段当故障复位。单主机先有界等待2ms，再进入HAL，避开其
     * 固定25ms BUSY等待。自旋上限仅作时基失效的第二道退出保护。 */
    start=HAL_GetTick();
    while(__HAL_I2C_GET_FLAG(&hi2c1,I2C_FLAG_BUSY)){
        if((uint32_t)(HAL_GetTick()-start)>=2U || ++spins>=65536U){break;}
    }
    elapsed=(uint32_t)(HAL_GetTick()-start);
    if(!__HAL_I2C_GET_FLAG(&hi2c1,I2C_FLAG_BUSY) &&
       elapsed<20U && HAL_I2C_Mem_Write(&hi2c1,0x78U,control,I2C_MEMADD_SIZE_8BIT,(uint8_t *)data,size,20U-elapsed)==HAL_OK){return 1;}
    /* F1 BUSY锁定：有界失败并清本外设，外部拉低仍失败，不伪称总线已恢复。
     * 下次Business周期重新初始化显示；不在本次错误内无限重试。 */
    (void)HAL_I2C_DeInit(&hi2c1);
    __HAL_RCC_I2C1_FORCE_RESET();__HAL_RCC_I2C1_RELEASE_RESET();
    if(HAL_I2C_Init(&hi2c1)!=HAL_OK){return 0;}
    return 0;
}

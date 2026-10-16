#include "app_temperature.h"
#include "app_model.h"
#include "adc.h"
static uint8_t calibrated;
int AppTemperature_Init(void)
{
    calibrated=(uint8_t)(HAL_ADCEx_Calibration_Start(&hadc1)==HAL_OK);
    /* CH16配置已打开TSVREFE；传感器稳定等待1ms，大于数据手册10us。 */
    HAL_Delay(1U);return calibrated;
}
int AppTemperature_Read(uint16_t *raw,int32_t *x10)
{
    uint32_t i,sum=0U,value;
    if(!raw || !x10 || !calibrated){return 0;}
    for(i=0U;i<16U;++i){
        if(HAL_ADC_Start(&hadc1)!=HAL_OK){(void)HAL_ADC_Stop(&hadc1);return 0;}
        if(HAL_ADC_PollForConversion(&hadc1,5U)!=HAL_OK){(void)HAL_ADC_Stop(&hadc1);return 0;}
        value=HAL_ADC_GetValue(&hadc1);
        if(HAL_ADC_Stop(&hadc1)!=HAL_OK || value>4095U){return 0;}
        sum+=value;
    }
    *raw=(uint16_t)((sum+8U)/16U);return AppModel_Temperature(*raw,x10);
}

#ifndef APP_TEMPERATURE_H
#define APP_TEMPERATURE_H
#include <stdint.h>
/* Business唯一owner；启动校准，16次239.5周期采样，每次最多5ms。失败不发布假温度。 */
int AppTemperature_Init(void);
int AppTemperature_Read(uint16_t *raw,int32_t *x10);
#endif

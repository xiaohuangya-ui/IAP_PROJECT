#ifndef APP_MODEL_H
#define APP_MODEL_H
#include <stdint.h>
/* 纯业务状态；任务在短临界区复制快照，不在临界区调用HAL/格式化。 */
typedef struct {
    uint32_t start_ms,business_seq,business_ms,adc_ok,oled_ok,uart_ms;
    int32_t temp_x10;
    uint16_t raw;
    uint8_t trial,uart_ready,fatal,temp_ok,oled_ready,update;
} AppModel;
typedef struct {int16_t samples[60];uint8_t count,next;} AppTrend;
int AppModel_Confirm(const AppModel *s,uint32_t now);
int AppModel_UartFresh(const AppModel *s,uint32_t now);
/* raw为0..4095；拒绝电源轨及超出F103结温工作解释范围的估算。 */
int AppModel_Temperature(uint16_t raw,int32_t *x10);
void AppTrend_Push(AppTrend *t,int32_t x10,int valid);
int16_t AppTrend_At(const AppTrend *t,uint32_t oldest_index);
#endif

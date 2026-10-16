#ifndef RECOVERY_H
#define RECOVERY_H
#include <stdint.h>
typedef struct {
    uint32_t high_a, high_b, config_pre, config_post;
    uint32_t cycle_start, cycle_end;
} RecoverySample;
/* 简介：前台、72MHz、MX_GPIO_Init之前采样；无 HAL Tick 依赖。
 * 参数：out 为诊断快照；CPU循环不承诺毫秒，cycle为DWT差值供实测。
 * 返回值：1 留Boot（低/混合/配置失败），0 两轮全高；只配置PB12。 */
int Recovery_Sample(RecoverySample *out);
#endif

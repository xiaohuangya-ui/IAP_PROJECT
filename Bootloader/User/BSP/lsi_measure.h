#ifndef LSI_MEASURE_H
#define LSI_MEASURE_H
#include <stdint.h>
typedef struct {uint32_t hz,bdcr_before,csr_before,prl,counter_before,counter_after,elapsed_ms,source_selected;} LsiMeasurement;
/* Phase8唯一RTC测频owner。要求HSE参考72MHz、前台IRQ开放。
 * RTCSEL为LSE/HSE则拒绝，绝不BDRST；0时明确选LSI，此选择不可撤销。
 * 已有LSI RTC不改PRL/CNT/ALR，以CNT+DIV测500ms；恢复RTCEN/DBP/时钟。
 * 所有同步/一致采样有期限；result保留原现场与是否改变RTCSEL。
 * 这是实机有副作用接口，只由用户受控运行的Phase8固件调用。 */
int Lsi_Measure(LsiMeasurement *result);
#endif

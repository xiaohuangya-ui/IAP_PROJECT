#ifndef DMA_CURSOR_H
#define DMA_CURSOR_H
#include <stdint.h>
/* CNDTR为0..512；相同位置去重。delta>256表示半圈服务契约已破坏，拒收。
 * 纯位置无法识别整圈覆盖，BSP另以独立CPU周期守卫约束服务间隔。 */
int DmaCursor_Advance(uint16_t *last, uint16_t remaining, uint16_t *delta);
#endif

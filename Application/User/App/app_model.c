/* 纯业务模型owner：健康判定、结温估算与60项趋势；无HAL/介质副作用。
 * AppTrend由零初始化开始，仅Business写入；count<=60、next<60。
 * 保留常数取模：本轮分支回绕经HOST等价验证，但AC5完整V2镜像收益0B。 */
#include "app_model.h"
int AppModel_UartFresh(const AppModel *s,uint32_t now)
{return s && s->uart_ready && (uint32_t)(now-s->uart_ms)<=20U;}
int AppModel_Confirm(const AppModel *s,uint32_t now)
{
    return s && s->trial && !s->fatal && !s->update &&
        (uint32_t)(now-s->start_ms)>=3000U && s->business_seq>=5U &&
        (uint32_t)(now-s->business_ms)<=1000U &&
        s->adc_ok && s->oled_ok && AppModel_UartFresh(s,now);
}
int AppModel_Temperature(uint16_t raw,int32_t *x10)
{
    int32_t value;
    if(!x10 || !raw || raw>=4095U){return 0;}
    /* 电压以10uV为单位，中间乘积最大1.352e9，不溢出uint32。 */
    value=250+(143000-(int32_t)((uint32_t)raw*330000U/4095U))/43;
    if(value < -400 || value > 1250){return 0;}
    *x10=value;return 1;
}
void AppTrend_Push(AppTrend *t,int32_t x10,int valid)
{
    if(!t || !valid || x10 < -400 || x10 > 1250){return;}
    t->samples[t->next]=(int16_t)x10;t->next=(uint8_t)((t->next+1U)%60U);
    if(t->count<60U){++t->count;}
}
int16_t AppTrend_At(const AppTrend *t,uint32_t i)
{
    uint32_t first;
    if(!t || i>=t->count){return 0;}
    first=t->count==60U?t->next:0U;return t->samples[(first+i)%60U];
}

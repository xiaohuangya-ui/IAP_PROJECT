#include "app_model.h"
#include "ssd1306.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls,fail_at,data_calls;static void *expected;
static int write_port(void *ctx,uint8_t control,const uint8_t *data,uint16_t size)
{
    assert(ctx==expected && data && size<=32U && size>0U);++calls;
    if(control==0x40U){assert(size==32U);++data_calls;}else{assert(control==0U);}
    return calls!=fail_at;
}
int main(void)
{
    AppModel s={0},b;AppTrend t={0};int32_t temp;unsigned raw,i,j;uint8_t copy[1024];
    s.trial=s.uart_ready=1;s.adc_ok=s.oled_ok=1;s.business_seq=5;s.uart_ms=s.business_ms=3000U;
    assert(AppModel_Confirm(&s,3000U));assert(!AppModel_Confirm(&s,2999U));
    for(i=0;i<8;++i){b=s;switch(i){case 0:b.trial=0;break;case 1:b.uart_ready=0;break;case 2:b.adc_ok=0;break;case 3:b.oled_ok=0;break;case 4:b.business_seq=4;break;case 5:b.fatal=1;break;case 6:b.update=1;break;default:b.uart_ms=2979U;break;}assert(!AppModel_Confirm(&b,3000U));}
    b=s;b.business_ms=1999U;assert(!AppModel_Confirm(&b,3000U));
    s.start_ms=0xFFFFF800U;s.uart_ms=s.business_ms=s.start_ms+3000U;assert(AppModel_Confirm(&s,s.uart_ms));
    assert(!AppModel_Confirm(NULL,0));assert(!AppModel_Temperature(0,&temp));assert(!AppModel_Temperature(4095,&temp));assert(!AppModel_Temperature(1775,NULL));
    for(raw=1;raw<4095;++raw){
        double exact=250.0+(143000.0-(double)raw*330000.0/4095.0)/43.0;
        int ok=AppModel_Temperature((uint16_t)raw,&temp);
        if(exact>=-399.0 && exact<=1249.0){assert(ok);}
        if(exact< -402.0 || exact>1252.0){assert(!ok);}
        if(ok){assert(temp>=-400 && temp<=1250);assert((double)temp-exact<1.1 && (double)temp-exact>-1.1);}
    }
    for(i=0;i<1000;++i){AppTrend_Push(&t,(int32_t)(i%100U),1);assert(t.count==(i<60U?i+1U:60U));for(j=0;j<t.count;++j){assert(AppTrend_At(&t,j)==(int16_t)((i+1U-t.count+j)%100U));}}
    {AppTrend old=t;AppTrend_Push(&t,30,0);AppTrend_Push(&t,32768,1);assert(!memcmp(&t,&old,sizeof(t)));}
    assert(AppTrend_At(&t,60U)==0);
    expected=&s;Ssd_Bind(write_port,expected);assert(Ssd_Init());assert(calls==1U);
    Ssd_Clear();Ssd_Pixel(-1,0);Ssd_Pixel(128,63);Ssd_Pixel(0,64);
    for(i=0;i<1024U;++i){assert(Ssd_Frame()[i]==0U);}
    Ssd_Pixel(127,63);assert(Ssd_Frame()[1023]==128U);
    Ssd_Text(0,0,"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");Ssd_Text(0,63,"ABCD");Ssd_Text(0xFFFFFFFFU,0,"A");
    memcpy(copy,Ssd_Frame(),1024);assert(Ssd_Flush());assert(data_calls==32U);assert(!memcmp(copy,Ssd_Frame(),1024));
    for(i=1;i<=33U;++i){calls=data_calls=0U;fail_at=i;assert(!Ssd_Flush());assert(calls==i);}
    Ssd_Bind(NULL,NULL);assert(!Ssd_Init() && !Ssd_Flush());
    puts("{\"result\":\"PASS\",\"temperature_raw_cases\":4094,\"trend_pushes\":1000,\"flush_failure_points\":33,\"health_negative_paths\":9}");return 0;
}

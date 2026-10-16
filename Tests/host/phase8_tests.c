#include "upgrade_log.h"
#include "store.h"
#include "watchdog_contract.h"
#include "text_reply.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,writes;static int cut=-1,read_fail;static uint8_t medium[16384],saved[16384];
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int Store_Read(StoreArea a,uint32_t o,uint8_t *p,uint32_t n)
{C(a==STORE_LOG&&o<sizeof(medium)&&n<=sizeof(medium)-o);if(read_fail){return 0;}memcpy(p,medium+o,n);return 1;}
int Store_Program(StoreArea a,uint32_t o,const uint8_t *p,uint32_t n)
{unsigned i;C(a==STORE_LOG&&o<sizeof(medium)&&n<=sizeof(medium)-o);for(i=0;i<n;++i){if(cut==0){return 0;}if(cut>0){--cut;}medium[o+i]&=p[i];++writes;}return 1;}
int Store_Erase(StoreArea a,uint32_t o,uint32_t n)
{unsigned i;C(a==STORE_LOG&&!(o%4096)&&n==4096&&o<=sizeof(medium)-n);for(i=0;i<n;++i){if(cut==0){return 0;}if(cut>0){--cut;}medium[o+i]=255;++writes;}return 1;}
static unsigned latest(unsigned *corrupt)
{uint32_t cursor=0;LogRecord r;unsigned seq=0,before=writes;int result;*corrupt=0;
 while((result=Log_ReadNext(&cursor,&r,corrupt))>0){if(r.sequence>seq){seq=r.sequence;}}C(result==0&&writes==before);return seq;}
int main(void)
{
    unsigned i,bad,before;uint16_t reload;WatchdogHealth h={0};TextReply t;char text[12];
    C(!Watchdog_Reload(29999,&reload));C(!Watchdog_Reload(60001,&reload));C(!Watchdog_Reload(40000,0));
    for(i=30000;i<=60000;++i){C(Watchdog_Reload(i,&reload));C(reload==(i+8)/16-1);}
    h.last_feed_ms=0xFFFFFF00U;h.last_business=0xFFFFFFFFU;
    C(Watchdog_Healthy(&h,100,0,1,0));C(!Watchdog_Healthy(&h,200,0,1,0));
    C(!Watchdog_Healthy(&h,200,1,0,0));C(!Watchdog_Healthy(&h,200,1,1,1));
    C(!Watchdog_Healthy(&h,2101,1,1,0));C(h.last_business==0&&h.last_feed_ms==100);
    C(!Watchdog_Healthy(&h,200,0xFFFFFFFFU,1,0));C(Watchdog_Healthy(&h,200,1,1,0));
    TextReply_Init(&t,text,sizeof(text));TextReply_U32(&t,0xFFFFFFFFU);C(!strcmp(TextReply_Result(&t),"4294967295"));
    TextReply_Add(&t,"x");C(!strcmp(TextReply_Result(&t),"4294967295x"));TextReply_Add(&t,"y");C(!strcmp(TextReply_Result(&t),"ERR RESPONSE_SIZE\r\n"));
    TextReply_Init(&t,text,2);TextReply_U32(&t,0);C(!strcmp(TextReply_Result(&t),"0"));
    memset(medium,255,sizeof(medium));C(Log_Append(LOG_START_DOWNLOAD,2,1,0,0));memcpy(saved,medium,sizeof(saved));
    for(i=0;i<32;++i){memcpy(medium,saved,sizeof(saved));cut=(int)i;C(!Log_Append(LOG_DOWNLOAD_VALID,5,2,0,0));cut=-1;
        C(latest(&bad)==1);C(bad==(i?1U:0U));C(Log_Append(LOG_ERROR,5,2,7,0));C(latest(&bad)==2);}
    memcpy(medium,saved,sizeof(saved));for(i=1;i<512;++i){C(Log_Append(LOG_ERROR,1,1,0,i));}
    C(latest(&bad)==512&&bad==0);memcpy(saved,medium,sizeof(saved));
    for(i=0;i<4128;i+=31){memcpy(medium,saved,sizeof(saved));cut=(int)i;C(!Log_Append(LOG_CONFIRM,1,2,0,0));cut=-1;C(latest(&bad)==512);}
    memcpy(medium,saved,sizeof(saved));C(Log_Append(LOG_CONFIRM,1,2,0,0));C(latest(&bad)==513);
    before=writes;read_fail=1;C(!Log_Append(LOG_ERROR,1,1,0,0));C(writes==before);read_fail=0;
    C(!Log_Append((LogEvent)0,1,1,0,0));C(!Log_Append(LOG_ERROR,10,1,0,0));
    printf("{\"result\":\"PASS\",\"checks\":%u,\"scope\":\"actual 32B log tears/rotation, watchdog eligibility and bounded text\"}\n",checks);return 0;
}

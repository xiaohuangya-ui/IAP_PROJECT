#include "app_history.h"
#include "image_contract.h"
#include "wire_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t medium[APP_HISTORY_BYTES],saved[APP_HISTORY_BYTES];
static int cut=-1,read_fail,start_fail;static unsigned writes,erases,starts;
static int start(void){++starts;return !start_fail;}
static int range(uint32_t off,uint32_t n){return n && off<sizeof(medium) && n<=sizeof(medium)-off;}
static int read_data(uint32_t off,uint8_t *p,uint32_t n)
{assert(p && range(off,n));if(read_fail)return 0;memcpy(p,medium+off,n);return 1;}
static int byte_write(uint32_t off,uint8_t v)
{if(cut==0)return 0;if(cut>0)--cut;medium[off]&=v;++writes;return 1;}
static int program(uint32_t off,const uint8_t *p,uint32_t n)
{uint32_t i;assert(p && range(off,n));for(i=0;i<n;i++){assert(medium[off+i]==255);if(!byte_write(off+i,p[i]))return 0;}return 1;}
static int erase(uint32_t off)
{uint32_t i;assert(!(off&4095U) && range(off,4096U));++erases;for(i=0;i<4096;i++){if(cut==0)return 0;if(cut>0)--cut;medium[off+i]=255;++writes;}return 1;}
static const AppHistoryPort port={start,read_data,program,erase};
static AppHistoryView get(void){AppHistoryView v;AppHistory_Get(&v);return v;}
static void boot(void)
{unsigned i;AppHistory_Bind(&port);for(i=0;i<64 && get().state!=HISTORY_READY && get().state!=HISTORY_ERROR;i++)AppHistory_Poll(0,0x20000,310,1775,1);}
static void sample(uint32_t n){AppHistory_Poll(n*60000U,0x20000,310+(int32_t)(n%10),1775,1);}
int main(void)
{
    unsigned i,j,before;AppHistoryView v;uint32_t stable;
    memset(medium,255,sizeof(medium));AppHistory_Bind(&port);
    for(i=0;i<100;i++)AppHistory_Poll(i*60000U,0x20000,310,1775,0);
    assert(!starts && !writes);boot();assert(get().state==HISTORY_READY && !writes);
    for(i=0;i<64;i++){
        memset(medium,255,sizeof(medium));boot();cut=(int)i;sample(1);assert(get().state==HISTORY_ERROR);cut=-1;
        boot();v=get();assert(v.sequence==0 && (v.state==HISTORY_READY || v.state==HISTORY_ERROR));
    }
    memset(medium,255,sizeof(medium));boot();
    sample(1);assert(get().sequence==1 && get().count==1 && !erases);
    memcpy(saved,medium,sizeof(medium));
    /* Every record byte tear; recovery must retain last committed sample. */
    for(i=0;i<32;i++){
        memcpy(medium,saved,sizeof(medium));boot();cut=(int)i;sample(1);assert(get().state==HISTORY_ERROR);cut=-1;
        boot();v=get();assert(v.state==HISTORY_READY && v.sequence==1);sample(1);assert(get().sequence==2);
    }
    memcpy(medium,saved,sizeof(medium));boot();
    for(i=2;i<=127;i++)sample(i);
    assert(get().sequence==127);memcpy(saved,medium,sizeof(medium));
    /* Rotation: every erase/header/record byte failure, then independent reboot. */
    for(i=0;i<4160;i++){
        memcpy(medium,saved,sizeof(medium));boot();cut=(int)i;sample(1);cut=-1;
        boot();v=get();assert(v.state==HISTORY_READY && v.sequence==127);
        sample(1);assert(get().state==HISTORY_READY && get().sequence==128);
    }
    memcpy(medium,saved,sizeof(medium));boot();sample(1);assert(get().sequence==128);
    /* More than all 256 sectors: actual ring erase and recovery of latest four. */
    for(i=2;i<=32600;i++)sample(i);
    stable=get().sequence;assert(stable==32727U);boot();v=get();assert(v.state==HISTORY_READY && v.sequence==stable && v.count==4);
    for(j=0;j<4;j++)assert(v.recent[j].sequence==stable-3+j);
    before=writes;AppHistory_Poll(60000,0,310,1775,1);AppHistory_Poll(60000,1,1251,1775,1);
    AppHistory_Poll(60000,1,310,0,1);AppHistory_Poll(60000,1,310,1775,0);assert(writes==before);
    /* Foreign/corrupt unformatted medium fails closed without erasing. */
    memset(medium,255,sizeof(medium));medium[0]=1;before=writes;boot();assert(get().state==HISTORY_ERROR && writes==before);
    memset(medium,255,sizeof(medium));medium[100]=1;boot();assert(get().state==HISTORY_ERROR);
    memset(medium,255,sizeof(medium));boot();read_fail=1;sample(1);assert(get().state==HISTORY_ERROR);read_fail=0;
    start_fail=1;boot();assert(get().state==HISTORY_ERROR);start_fail=0;
    AppHistory_Bind(NULL);assert(get().state==HISTORY_ERROR);
    /* Header generation exhaustion and duplicate newest generation. */
    memset(medium,255,sizeof(medium));boot();sample(1);memcpy(medium+4096,medium,32);boot();assert(get().state==HISTORY_ERROR);
    memset(medium,255,sizeof(medium));boot();sample(1);
    Wire_WriteLE32(medium+4,0xFFFFFFFDU);Wire_WriteLE32(medium+28,CRC32_IEEE(medium,28));
    boot();assert(get().state==HISTORY_READY);
    puts("PASS: 64 initial header/record tears, 32 record tears, 4160 rotation byte cuts, full 256-sector wrap, reboot/history/invalid/IO guards");
    return 0;
}

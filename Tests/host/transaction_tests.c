#include "iap_fsm.h"
#include "internal_flash_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t medium[0x26000],flash[0xC000],saved[8192],old_flash[0xC000],file[1061];
static long cut=-1;static unsigned checks,flash_changes,medium_changes;
#ifdef IAP_PHASE8_ENABLED
static unsigned progress_count;
void Iwdg_Progress(void){++progress_count;}
#endif
static const unsigned base[]={0,0x10000,0x20000,0x21000,0x22000};
static const unsigned size[]={65536,65536,4096,4096,16384};
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d\n",__LINE__);exit(1);}}while(0)
static int step(void){if(cut==0){return 0;}if(cut>0){--cut;}return 1;}
int Store_Read(StoreArea a,uint32_t o,uint8_t *p,uint32_t n)
{C((unsigned)a<5&&o<size[a]&&n<=size[a]-o);memcpy(p,medium+base[a]+o,n);return 1;}
int Store_Program(StoreArea a,uint32_t o,const uint8_t *p,uint32_t n)
{uint32_t i;C((unsigned)a<5&&o<size[a]&&n<=size[a]-o);
 for(i=0;i<n;++i){if(!step()){return 0;}medium[base[a]+o+i]&=p[i];++medium_changes;}return 1;}
int Store_Erase(StoreArea a,uint32_t o,uint32_t n)
{uint32_t i;C((unsigned)a<5&&o<size[a]&&n<=size[a]-o);for(i=0;i<n;++i){if(!step()){return 0;}medium[base[a]+o+i]=255;++medium_changes;}return 1;}
int Store_CRC(StoreArea a,uint32_t o,uint32_t n,uint32_t *crc)
{C((unsigned)a<5&&o<size[a]&&n<=size[a]-o);*crc=CRC32_IEEE(medium+base[a]+o,n);return 1;}
int FlashPort_Context(void){return 1;}
const uint8_t *FlashPort_View(uint32_t a){return flash+a-APP_BASE;}
int FlashPort_Read(uint32_t a,uint8_t *p,uint32_t n)
{C(a>=APP_BASE&&n<=APP_LIMIT-(a-APP_BASE));memcpy(p,flash+a-APP_BASE,n);return 1;}
FlashResult FlashPort_Operation(uint32_t a,uint16_t v,int erase)
{
    C(a>=APP_BASE&&a<APP_BASE+APP_LIMIT);if(!step()){return IF_TIMEOUT;}++flash_changes;
    if(erase){memset(flash+a-APP_BASE,255,1024);}else{flash[a-APP_BASE]=(uint8_t)v;flash[a-APP_BASE+1]=(uint8_t)(v>>8);}return IF_OK;
}
static void put(uint8_t *p,uint32_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);p[2]=(uint8_t)(x>>16);p[3]=(uint8_t)(x>>24);}
static ImageIdentity seed(void)
{
    ImageIdentity id;unsigned i;cut=-1;
    memset(medium,255,sizeof(medium));for(i=0;i<sizeof(flash);++i){flash[i]=(uint8_t)(i*13+7);}
    put(flash,0x20005000);put(flash+4,APP_BASE+9);memcpy(old_flash,flash,sizeof(flash));
    id.version=0x10000;id.size=777;id.crc32=CRC32_IEEE(flash,id.size);
    C(Meta_Provision(&id));C(Iap_Load());return id;
}
static ImageIdentity download(void)
{
    ImageIdentity id;unsigned i;
    for(i=32;i<sizeof(file);++i){file[i]=(uint8_t)i;}
    put(file+32,0x20005000);put(file+36,APP_BASE+9);
    id.version=0x20000;id.size=sizeof(file)-32;id.crc32=CRC32_IEEE(file+32,id.size);
    put(file,0x46574D47);put(file+4,1);put(file+8,id.version);put(file+12,id.size);put(file+16,id.crc32);
    put(file+20,APP_BASE);put(file+24,0);put(file+28,CRC32_IEEE(file,28));
    C(Iap_Enter());C(Iap_Record()->state==RECEIVING);C(Iap_Begin(sizeof(file)));
    C(Iap_Write(0,file,1024));C(Iap_Write(1024,file+1024,sizeof(file)-1024));C(Iap_Finish(&id));
    return id;
}
int main(void)
{
    ImageIdentity old,newid;MetaScan s;Metadata r;BootFacts f;uint8_t bytes[72];unsigned i,old_changes;int ok;
    old=seed();C(!Meta_Provision(&old));C(Meta_Scan(&s)&&s.found&&s.record.state==CONFIRMED);
    memcpy(saved,medium+0x20000,sizeof(saved));
    /* Journal每个字节掉电：旧权威必须仍可扫描，脏槽不允许覆盖。 */
    for(i=0;i<72;++i){
        memcpy(medium+0x20000,saved,sizeof(saved));C(Meta_Scan(&s));r=s.record;r.state=RECEIVING;
        cut=(long)i;C(!Meta_Commit(&r));cut=-1;C(Meta_Scan(&s)&&s.found&&s.record.sequence==1);
        r=s.record;r.state=RECEIVING;C(Meta_Commit(&r));C(r.sequence==2);
    }
    seed();C(Meta_Scan(&s));r=s.record;
    for(i=0;i<55;++i){r.error=i;C(Meta_Commit(&r));}
    C(Meta_Scan(&s)&&s.slot==55);memcpy(saved,medium+0x20000,sizeof(saved));
    for(i=0;i<4170;i+=67){
        memcpy(medium+0x20000,saved,sizeof(saved));C(Meta_Scan(&s));r=s.record;
        cut=(long)i;ok=Meta_Commit(&r);cut=-1;C(Meta_Scan(&s)&&s.found);
        C(s.record.sequence==56||s.record.sequence==57);if(ok){C(s.record.sequence==57);}
    }
    seed();C(Meta_Scan(&s));r=s.record;r.sequence=0xFFFFFFFEU;Meta_Encode(&r,bytes);memcpy(medium+0x20000,bytes,72);C(!Meta_Commit(&r));
    seed();C(Meta_Scan(&s));r=s.record;r.error=99;Meta_Encode(&r,bytes);memcpy(medium+0x21000,bytes,72);C(!Meta_Scan(&s));
    seed();newid=download();old_changes=flash_changes;medium[100]^=1;C(!Iap_Recover());C(flash_changes==old_changes);C(!memcmp(flash,old_flash,sizeof(flash)));
    seed();newid=download();C(Iap_Recover());C(Iap_Record()->state==PENDING_CONFIRM);C(InternalFlash_Verify(&newid));C(Backup_Verify(&Iap_Record()->backup));
    memset(&f,0,sizeof(f));f.pending_ok=f.backup_ok=1;
    C(BootPolicy_Decide(Iap_Record(),&f)==IAP_TRIAL);
    for(i=0;i<3;++i){C(Iap_TrialCommit());C(Iap_Record()->attempts==i+1);C(Iap_Load());}
    C(!Iap_TrialCommit());C(BootPolicy_Decide(Iap_Record(),&f)==IAP_ROLLBACK);
    f.confirm=f.trial_mark=1;C(BootPolicy_Decide(Iap_Record(),&f)==IAP_CONFIRM);C(Iap_Confirm());C(Iap_Record()->active.version==newid.version);
    seed();download();C(Iap_Recover());C(Iap_TrialCommit());C(Iap_Rollback());C(!memcmp(flash,old_flash,sizeof(flash)));C(Iap_Record()->state==CONFIRMED);
    /* 独立物理CRC：有效区外被破坏，回滚必须拒绝且不能擦内部。 */
    seed();download();C(Iap_Recover());medium[0x10000+40000]^=1;old_changes=flash_changes;
    C(!Iap_Rollback());C(flash_changes==old_changes);
    /* 九态决策优先级；半写INSTALLING和ROLLBACK不得被PB12绕过。 */
    C(Meta_Scan(&s));r=s.record;memset(&f,0,sizeof(f));f.hold=f.update=1;
    for(i=CONFIRMED;i<=FAILED;++i){r.state=i;r.attempts=0;
        if(i==RECEIVING){r.download_valid=0;}else{r.download_valid=1;}
        if(i==DOWNLOADED||i==VERIFYING||i==BACKING_UP||i==INSTALLING||i==ROLLBACK){C(BootPolicy_Decide(&r,&f)==IAP_RECOVER);}
        else{C(BootPolicy_Decide(&r,&f)==IAP_HOLD);}
    }
#ifdef IAP_PHASE8_ENABLED
    /* Journal成功后日志失败：不得撤回已经可信的RECEIVING或对外假报未提交。 */
    seed();cut=72;C(Iap_Enter());cut=-1;C(Iap_LogFault());C(Iap_Load());C(Iap_Record()->state==RECEIVING);
    C(progress_count>0);
#endif
    printf("{\"checks\":%u,\"result\":\"PASS\",\"scope\":\"actual Journal/FSM/Policy/Install with medium faults\"}\n",checks);return 0;
}

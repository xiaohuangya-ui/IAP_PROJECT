/* 复用原介质/Flash端口，实际Release包连接标准YModem→Journal→备份/安装/Trial。 */
#define main legacy_main
#include "transaction_tests.c"
#undef main
#include "ymodem.h"
#include "crc16.h"
#include "app_model.h"
static uint8_t package[49184],v1[49184],start_medium9[sizeof(medium)],start_flash9[sizeof(flash)];
static unsigned package_size,v1_size;static YModem ym;
static uint32_t clock9(void *p){(void)p;return 0;}
static int tx9(void *p,const uint8_t *b,uint16_t n){(void)p;C(b && n && n<=2U);return 1;}
static int begin9(void *p,uint32_t n){(void)p;return Iap_Begin(n);}
static int write9(void *p,uint32_t o,const uint8_t *b,uint16_t n){(void)p;return Iap_Write(o,b,n);}
static int finish9(void *p,const ImageIdentity *id){(void)p;return Iap_Finish(id);}
static void abort9(void *p){(void)p;Iap_Abort();}
static void send_byte(uint8_t byte){YModem_Poll(&ym);C(YModem_CanRead(&ym));YModem_Feed(&ym,byte);YModem_Poll(&ym);}
static void packet9(uint8_t number,const uint8_t *p,unsigned n)
{
    unsigned i;uint16_t crc=0;send_byte(n==128U?1:2);send_byte(number);send_byte((uint8_t)~number);
    /* 独立位循环sender，不调用被测CRC16来制造期望包。 */
    for(i=0;i<n;++i){unsigned bit;crc^=(uint16_t)p[i]<<8;for(bit=0;bit<8;++bit){crc=(uint16_t)((crc<<1)^((crc&0x8000U)?0x1021U:0U));}send_byte(p[i]);}
    send_byte((uint8_t)(crc>>8));send_byte((uint8_t)crc);
}
static ImageIdentity read_package(const char *path,uint8_t *bytes,unsigned *length)
{
    FILE *f=fopen(path,"rb");ImageIdentity id;size_t n;C(f!=NULL);n=fread(bytes,1,49184U,f);C(feof(f));fclose(f);
    *length=(unsigned)n;C(FirmwareHeader_Decode(bytes,32U,(uint32_t)n,&id));C(CRC32_IEEE(bytes+32,id.size)==id.crc32);return id;
}
static void seed9(const ImageIdentity *id)
{
    cut=-1;memset(medium,255,sizeof(medium));memset(flash,255,sizeof(flash));memcpy(flash,v1+32,id->size);
    memcpy(old_flash,flash,sizeof(flash));C(Meta_Provision(id));C(Iap_Load());
}
static void download9(const ImageIdentity *id,int ymodem)
{
    unsigned o,n;uint8_t block[1024];uint8_t number=1;YmPort port={0,begin9,write9,finish9,abort9,tx9,clock9};
    C(Iap_Enter());
    if(ymodem){
        C(YModem_Start(&ym,&port));YModem_Poll(&ym);memset(block,0,128);memcpy(block,"release.fw",10);
        (void)snprintf((char *)block+11,100,"%u",package_size);packet9(0,block,128);
        for(o=0;o<package_size;o+=n){n=package_size-o;if(n>1024U){n=1024U;}memset(block,26,sizeof(block));memcpy(block,package+o,n);packet9(number++,block,1024);}
        send_byte(4);send_byte(4);memset(block,0,128);packet9(0,block,128);C(ym.state==YM_DONE);
    }else{
        C(Iap_Begin(package_size));for(o=0;o<package_size;o+=n){n=package_size-o;if(n>1024U){n=1024U;}C(Iap_Write(o,package+o,(uint16_t)n));}C(Iap_Finish(id));
    }
    C(Iap_Record()->state==DOWNLOADED);
}
int main(int argc,char **argv)
{
    ImageIdentity old,candidate;BootFacts f;AppModel health;unsigned i;long cuts[]={0,1,67,68,71,72,143,215,287,4096,65536,80000,114900,115200,120000,130000};
    C(argc==3 || argc==4);old=read_package(argv[1],v1,&v1_size);candidate=read_package(argv[2],package,&package_size);
    seed9(&old);download9(&candidate,1);C(Iap_Recover());C(InternalFlash_Verify(&candidate));C(!memcmp(medium+0x10000,old_flash,APP_LIMIT));
    memset(&f,0,sizeof(f));f.pending_ok=f.backup_ok=1;
    for(i=0;i<3U;++i){C(BootPolicy_Decide(Iap_Record(),&f)==IAP_TRIAL);C(Iap_TrialCommit());C(Iap_Load());}
    memset(&health,0,sizeof(health));health.trial=health.uart_ready=1;health.business_seq=6;health.uart_ms=health.business_ms=3250;health.adc_ok=health.oled_ok=1;
    if(argc==4){health.business_seq=0;C(!AppModel_Confirm(&health,3250));C(BootPolicy_Decide(Iap_Record(),&f)==IAP_ROLLBACK);C(Iap_Rollback());C(!memcmp(flash,old_flash,APP_LIMIT));}
    else{C(AppModel_Confirm(&health,3250));f.confirm=f.trial_mark=1;C(BootPolicy_Decide(Iap_Record(),&f)==IAP_CONFIRM);C(Iap_Confirm());C(Iap_Record()->active.crc32==candidate.crc32);}
    /* 独立V1基线，不能复用已确认V2的Backup来声称旧版一定为V1。 */
    seed9(&old);download9(&candidate,0);C(Iap_Recover());for(i=0;i<3U;++i){C(Iap_TrialCommit());C(Iap_Load());}
    f.confirm=f.trial_mark=0;C(BootPolicy_Decide(Iap_Record(),&f)==IAP_ROLLBACK);C(!Iap_TrialCommit());C(Iap_Rollback());C(!memcmp(flash,old_flash,APP_LIMIT));
    seed9(&old);download9(&candidate,0);memcpy(start_medium9,medium,sizeof(medium));memcpy(start_flash9,flash,sizeof(flash));
    for(i=0;i<sizeof(cuts)/sizeof(cuts[0]);++i){
        memcpy(medium,start_medium9,sizeof(medium));memcpy(flash,start_flash9,sizeof(flash));cut=-1;C(Iap_Load());cut=cuts[i];(void)Iap_Recover();cut=-1;
        C(Iap_Load());C(Iap_Recover());C(Iap_Record()->state==PENDING_CONFIRM);C(InternalFlash_Verify(&candidate));C(Backup_Verify(&Iap_Record()->backup));
    }
    printf("{\"result\":\"PASS\",\"checks\":%u,\"actual_package_bytes\":%u,\"recovery_cuts\":16,\"third_trial_confirmation\":%s,\"rollback_bytes\":49152}\n",checks,package_size,argc==4?"false":"true");return 0;
}

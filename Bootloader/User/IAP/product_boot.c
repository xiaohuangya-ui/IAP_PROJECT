#include "control_zi.h"
#include "product_boot.h"
#include "iap_fsm.h"
#include "boot_protocol.h"
#include "backup_domain.h"
#include "recovery.h"
#include "jump.h"
#include "spi_bus.h"
#include "w25q64.h"
#include "uart_port.h"
#include "internal_flash.h"
#include "stm32f1xx_hal.h"
#include "text_reply.h"
#include <string.h>
#ifdef IAP_PHASE8_ENABLED
#include "boot_iwdg.h"
#include "upgrade_log.h"
static BOOT_CONTROL_ZI uint32_t log_cursor,log_corrupt;static BOOT_CONTROL_ZI int log_active;
/* 同步、唯一IWDG启动入口；重复决策仍复核Running，不合并介质验证时间点。 */
static __attribute__((noinline)) int watchdog_ready(void)
{return Iwdg_Running()||Iwdg_StartMeasured();}
#endif
static BOOT_CONTROL_ZI int hold,transaction_done;
static char text[192];
/* 固定字段对共用一个小入口，防止Size/O3把重复的两个调用展开。 */
static __attribute__((noinline)) void field(TextReply *out,const char *label,uint32_t value)
{TextReply_Add(out,label);TextReply_U32(out,value);}
#ifdef W25_PRODUCT_BINDING
void Product_StorageProgress(void){
#else
static void progress(void){
#endif
#ifdef IAP_PHASE8_ENABLED
    Iwdg_Progress();
#endif
}
void Product_EarlyInit(void){RecoverySample sample;hold=Recovery_Sample(&sample);}
#ifndef PROTOCOL_PRODUCT_BINDING
static int begin(void *p,uint32_t n){(void)p;return Iap_Begin(n);}
static int write_data(void *p,uint32_t o,const uint8_t *b,uint16_t n){(void)p;return Iap_Write(o,b,n);}
static int finish(void *p,const ImageIdentity *i){(void)p;return Iap_Finish(i);}
static void abort_rx(void *p){(void)p;Iap_Abort();}
static int tx(void *p,const uint8_t *b,uint16_t n){(void)p;return Uart_TxSubmit(b,n);}
static uint32_t clock_ms(void *p){(void)p;return HAL_GetTick();}
#endif
static void closed(int ok)
{
    transaction_done=ok;
#ifdef IAP_PHASE8_ENABLED
    /* 链路故障作废正在输出的LOG会话；新命令必须重新建立维护边界。 */
    if(!ok){log_active=0;}
#endif
}
#ifdef IAP_PHASE8_ENABLED
static const char *log_next(void)
{
    LogRecord r;TextReply out;int result;
    if(!log_active){return 0;}
    TextReply_Init(&out,text,sizeof(text));result=Log_ReadNext(&log_cursor,&r,&log_corrupt);
    if(result<0){log_active=0;return "ERR LOG_READ\r\n";}
    if(!result){log_active=0;TextReply_Add(&out,"LOG END LOG_CORRUPT=");TextReply_U32(&out,log_corrupt);}
    else{
        field(&out,"LOG SEQ=",r.sequence);
        field(&out," EVENT=",r.event);
        field(&out," STATE=",r.state);
        field(&out," VERSION=",r.version);
        field(&out," ERROR=",r.error);
        field(&out," DETAIL=",r.detail);
    }
    TextReply_Add(&out,"\r\n");return TextReply_Result(&out);
}
#endif
static const char *query(Command cmd)
{
    const Metadata *r=Iap_Record();TextReply out;
#ifdef IAP_PHASE8_ENABLED
    if(cmd==CMD_IAP_LOG){log_cursor=log_corrupt=0;log_active=1;return "LOG BEGIN\r\n";}
#endif
    if(!r){return "IAP STATE=UNKNOWN ERROR=METADATA\r\n";}
    TextReply_Init(&out,text,sizeof(text));
    if(cmd==CMD_IAP_VER){TextReply_Add(&out,"BOOT UART-IAP VER=1 ACTIVE=");TextReply_U32(&out,r->active.version);}
    else if(cmd==CMD_IAP_STATUS){
        field(&out,"IAP STATE=",r->state);
        field(&out," DOWNLOAD=",r->download_valid);
        field(&out," BACKUP=",r->backup_valid);
        field(&out," ATTEMPTS=",r->attempts);
        field(&out," ERROR=",r->error);
        field(&out," ACTIVE=",r->active.version);
        field(&out," PENDING=",r->pending.version);
#ifdef IAP_PHASE8_ENABLED
        field(&out," LOG_ERROR=",Iap_LogFault());
#endif
    }
    else{return "ERR NOT_READY\r\n";}
    TextReply_Add(&out,"\r\n");return TextReply_Result(&out);
}
#ifdef PROTOCOL_PRODUCT_BINDING
int Product_ControlEnter(void){return Iap_Enter();}
const char *Product_ControlQuery(Command cmd){return query(cmd);}
void Product_ControlClosed(int ok){closed(ok);}
#ifdef IAP_PHASE8_ENABLED
const char *Product_ControlNextText(void){return log_next();}
#endif
#endif
static void boot_decision(void)
{
    const Metadata *r;const ImageIdentity *image;BootFacts f;BootAction a;BackupSnapshot b;JumpTarget target;
    if(!Iap_Load()){return;}
#ifdef IAP_PHASE8_ENABLED
    r=Iap_Record();
    if((uint32_t)(r->state-DOWNLOADED)<=(uint32_t)(INSTALLING-DOWNLOADED)||r->state==ROLLBACK){
        if(!watchdog_ready()){return;}Iap_LogRecovery();
    }
#endif
    if(!Iap_Recover()){return;}
    r=Iap_Record();memset(&f,0,sizeof(f));Backup_ReadSnapshot(&b);
    f.hold=hold;f.update=b.dr1==BKP_UPDATE_REQ;f.confirm=b.dr2==BKP_CONFIRM_REQ;f.trial_mark=b.dr3==BKP_TRIAL_MARK;
    f.active_ok=InternalFlash_Verify(&r->active);f.pending_ok=InternalFlash_Verify(&r->pending);
    if(r->backup_valid){f.backup_ok=Backup_Verify(&r->backup);}
    if(r->state==PENDING_CONFIRM&&(!f.pending_ok||r->attempts==3U)&&
       !(f.confirm&&f.trial_mark&&f.pending_ok)&&!f.backup_ok){(void)Iap_Fail(3U);return;}
    a=BootPolicy_Decide(r,&f);
    /* 一次性请求消费保留本次快照，未通过写回核验不得跳转。 */
    if(!Backup_BootWrite(1,0)||!Backup_BootWrite(2,0)){return;}
#ifdef IAP_PHASE8_ENABLED
    if(a==IAP_CONFIRM||a==IAP_ROLLBACK){
        if(a==IAP_CONFIRM){if(!Iap_Confirm()){return;}}
        else {if(!watchdog_ready()||!Iap_Rollback()){return;}}
        r=Iap_Record();a=hold||f.update?IAP_HOLD:IAP_CONFIRMED;
    }
    if(a!=IAP_TRIAL&&a!=IAP_CONFIRMED){return;}
    if(!watchdog_ready()){return;}
    if(a==IAP_TRIAL){
        if(!Iap_TrialCommit()||!Backup_BootWrite(3,BKP_TRIAL_MARK)){return;}
        r=Iap_Record();image=&r->pending;
    }else
#endif
    {
        if(a!=IAP_CONFIRMED||!Backup_BootWrite(3,0)){return;}
        image=&r->active;
    }
    /* 本次处理结果仅选择身份，不缓存介质可信性；Jump独立全量复验。
     * Trial提交及DR3写回均成功后才进入共同最终守卫。 */
    if(Jump_Prepare(image,&target)){Jump_Commit(&target);}
    /* Phase7只准备持久Pending；Phase8才启用Trial/IWDG。 */
}
void Product_Main(void)
{
    /* 固定产品一次显式绑定控制与YModem；Generic仍复制静态生命周期端口。
     * 两种配置不混用，不以产品特化静默替换自定义回调。 */
#ifndef W25_PRODUCT_BINDING
    static const W25Port device={HAL_GetTick,Uart_Service,progress};
#endif
#ifndef PROTOCOL_PRODUCT_BINDING
    static const YmPort sink={0,begin,write_data,finish,abort_rx,tx,clock_ms};
#endif
#ifndef PROTOCOL_PRODUCT_BINDING
    static const IapProtocolControl ctl={Iap_Enter,query,closed
#ifdef IAP_PHASE8_ENABLED
        ,log_next
#endif
    };
#endif
    if(SpiBus_Init()&&
#ifdef W25_PRODUCT_BINDING
       W25_InitProduct()
#else
       W25_Init(&device)
#endif
       ){boot_decision();}
#ifdef PROTOCOL_PRODUCT_BINDING
    (void)BootProtocol_BindProduct();
#else
    (void)BootProtocol_SetControl(&ctl);
    (void)BootProtocol_SetReceiver(&sink);
#endif
    BootProtocol_Init();
    for(;;){
        BootProtocol_Poll();
        if(transaction_done&&Uart_TxComplete()){
            transaction_done=0;
            if(Uart_PauseReceive()){
#ifdef IAP_PHASE8_ENABLED
                if(!watchdog_ready()){(void)Uart_ResumeReceive();continue;}
                /* 决策自身Load/Recover；RX保持暂停直至跳转准备完成。
                 * Commit不返回；任何拒绝路径回来后才恢复维护接收。 */
                boot_decision();
#else
                (void)Iap_Recover();
#endif
                (void)Uart_ResumeReceive();
            }
        }
    }
}

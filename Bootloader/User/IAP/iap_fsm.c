/* Boot事务FSM唯一owner；协议只投递数据，Store/BSP负责获授权的介质操作。
 * 前台同步、不可重入；持久九态以Metadata为准，BKP只是一次性请求。 */
#include "control_zi.h"
#include "iap_fsm.h"
#include "internal_flash.h"
#include <string.h>
#ifdef IAP_PHASE8_ENABLED
#include "upgrade_log.h"
static BOOT_CONTROL_ZI uint32_t log_fault;
uint32_t Iap_LogFault(void){return log_fault;}
static void event(LogEvent e,const Metadata *r)
{if(!Log_Append(e,r->state,r->state==CONFIRMED?r->active.version:r->pending.version,r->error,r->attempts)){log_fault=1;}}
#endif
static Metadata current,draft_record;static BOOT_CONTROL_ZI uint8_t loaded;
/* 私有草稿仅在前台同步FSM调用内借用；介质Service/Progress不得重入FSM。
 * Journal提交时仍保存旧current的小字段，成功后才更新current并写日志。
 * 失败退出不继续使用草稿；下一入口总从当前持久快照重新复制。 */
static __attribute__((noinline)) Metadata *draft(void)
{draft_record=current;return &draft_record;}
static BOOT_CONTROL_ZI uint32_t file_size,received;
static int equal(const ImageIdentity *a,const ImageIdentity *b)
{return a->version==b->version&&a->size==b->size&&a->crc32==b->crc32;}
static int commit(Metadata *r)
{
#ifdef IAP_PHASE8_ENABLED
    /* 日志只需提交前的三个字段，不在嵌套持久提交路径复制整份记录。 */
    uint32_t before_state=current.state;
    uint8_t before_download=current.download_valid,before_backup=current.backup_valid;
    LogEvent e;
#endif
    if(!loaded||!Meta_Commit(r)){loaded=0U;return 0;}
    current=*r;
#ifdef IAP_PHASE8_ENABLED
    /* 固定事件用小表；三个有历史依赖的状态仍显式判定，不能按state抹平。
     * 非法状态和无事件态用0哨兵；Journal/current/Log顺序不变。 */
    {
        static const uint8_t fixed_events[]={LOG_CONFIRM,LOG_START_DOWNLOAD,0,0,0,
            LOG_INSTALL_START,LOG_INSTALL_OK,LOG_ROLLBACK_START,LOG_ERROR};
        e=r->state>=CONFIRMED&&r->state<=FAILED?(LogEvent)fixed_events[r->state-CONFIRMED]:(LogEvent)0;
        if(r->state==BACKING_UP){
            if(!before_download&&r->download_valid){e=LOG_DOWNLOAD_VALID;}
            else if(!before_backup&&r->backup_valid){e=LOG_BACKUP_VALID;}
        }
        else if(r->state==PENDING_CONFIRM&&r->attempts){e=LOG_TRIAL_START;}
        else if(r->state==CONFIRMED&&before_state==ROLLBACK){e=LOG_ROLLBACK_OK;}
    }
    if(e){event(e,r);}

#endif
    return 1;
}
#ifdef IAP_PHASE8_ENABLED
void Iap_LogRecovery(void){if(loaded){event(LOG_POWERLOSS_RECOVERY,&current);}}
#endif
int Iap_Load(void)
{MetaScan s;loaded=0U;if(!Meta_Scan(&s)||!s.found){return 0;}current=s.record;loaded=1U;return 1;}
const Metadata *Iap_Record(void){return loaded?&current:0;}
int Iap_Fail(uint32_t error)
{Metadata *r;if(!loaded){return 0;}r=draft();r->state=FAILED;r->attempts=0;r->error=error;return commit(r);}
BootAction BootPolicy_Decide(const Metadata *r,const BootFacts *f)
{
    if(!r||!f||!Meta_Valid(r)||r->sequence>=0xFFFFFFFEU){return IAP_HOLD;}
    switch(r->state){
    case DOWNLOADED:case VERIFYING:case BACKING_UP:case INSTALLING:case ROLLBACK:return IAP_RECOVER;
    case FAILED:case RECEIVING:return IAP_HOLD;
    case PENDING_CONFIRM:
        if(f->confirm&&f->trial_mark&&r->attempts&&f->pending_ok){return IAP_CONFIRM;}
        if(!f->pending_ok||r->attempts==3U){return f->backup_ok?IAP_ROLLBACK:IAP_HOLD;}
        if(f->hold||f->update){return IAP_HOLD;}return IAP_TRIAL;
    case CONFIRMED:return f->active_ok&&!f->hold&&!f->update?IAP_CONFIRMED:IAP_HOLD;
    default:return IAP_HOLD;
    }
}
int Iap_Enter(void)
{
    Metadata *r;
    if(!loaded||(current.state!=CONFIRMED&&current.state!=RECEIVING)||!InternalFlash_Verify(&current.active)){return 0;}
    r=draft();r->state=RECEIVING;r->download_valid=0;r->backup_valid=0;r->attempts=0;r->error=0;
    memset(&r->pending,0,sizeof(r->pending));file_size=received=0;return commit(r);
}
int Iap_Begin(uint32_t n)
{
    if(!loaded||current.state!=RECEIVING||file_size||(uint32_t)(n-40U)>APP_LIMIT-8U){return 0;}
    if(!Store_Erase(STORE_DOWNLOAD,0,65536U)){return 0;}
    file_size=n;received=0;return 1;
}
int Iap_Write(uint32_t off,const uint8_t *p,uint16_t n)
{
    ImageIdentity id;
    if(!loaded||current.state!=RECEIVING||!file_size||off!=received||!n||n>file_size-received){return 0;}
    if(!off&&(!FirmwareHeader_Decode(p,n,file_size,&id)||id.size<8U||id.version<current.active.version)){return 0;}
    if(!Store_Program(STORE_DOWNLOAD,off,p,n)){return 0;}received+=n;return 1;
}
int Iap_Finish(const ImageIdentity *id)
{
    Metadata *r;
    if(!loaded||current.state!=RECEIVING||!id||received!=file_size||file_size!=32U+id->size||id->version<current.active.version){return 0;}
    r=draft();r->pending=*id;r->state=DOWNLOADED;
    return commit(r); /* 只是完整协议结束，尚未valid，回读在恢复服务执行。 */
}
void Iap_Abort(void){file_size=received=0;}
static int download_ok(void)
{ImageIdentity id;return Download_Verify(32U+current.pending.size,&id)&&equal(&id,&current.pending);}
static int install(StoreArea area,uint32_t offset,uint32_t n,const ImageIdentity *id)
{
    uint8_t b[256];uint32_t off,k;
    if(InternalFlash_Erase(APP_BASE,(n+1023U)&~1023U)!=IF_OK){return 0;}
    for(off=0;off<n;off+=k){k=n-off;if(k>sizeof(b)){k=sizeof(b);}
        if(!Store_Read(area,offset+off,b,k)||InternalFlash_Program(APP_BASE+off,b,k)!=IF_OK){return 0;}}
    return InternalFlash_Verify(id);
}
/* 仅合并已独立复验之后的CONFIRMED字段装配。调用者仍负责其介质时间点的
 * 完整校验；Journal成功后current更新和日志顺序继续由commit独占。 */
static __attribute__((noinline)) int commit_confirmed(const ImageIdentity *id)
{
    Metadata *r=draft();r->active=*id;r->state=CONFIRMED;r->attempts=0;
    r->download_valid=r->backup_valid=0;r->error=0;return commit(r);
}
int Iap_Rollback(void)
{
    Metadata *r;ImageIdentity physical;
    if(!loaded||(current.state!=INSTALLING&&current.state!=PENDING_CONFIRM&&current.state!=ROLLBACK)||
       !current.backup_valid){return 0;}
    if(!Backup_Verify(&current.backup)){(void)Iap_Fail(3U);return 0;}
    r=draft();r->state=ROLLBACK;r->attempts=0;
    if(!commit(r)){return 0;}
    physical=current.backup.image;physical.size=APP_LIMIT;physical.crc32=current.backup.physical_crc;
    if(!install(STORE_BACKUP,0,APP_LIMIT,&physical)||!InternalFlash_Verify(&current.backup.image)){return 0;}
    return commit_confirmed(&current.backup.image);
}
int Iap_Recover(void)
{
    Metadata *r;BackupIdentity backup;unsigned steps;
    /* 每个状态最多推进一次；无无限重试或无进展喂狗。 */
    for(steps=0;loaded&&steps<8U;++steps){
        r=draft();
        switch(r->state){
        /* DOWNLOADED不是“暂存待按键确认”：恢复入口会继续验证/备份/安装。 */
        case DOWNLOADED:r->state=VERIFYING;if(!commit(r)){return 0;}break;
        case VERIFYING:
            if(!download_ok()){(void)Iap_Fail(1U);return 0;}
            r->download_valid=1;r->state=BACKING_UP;r->backup_valid=0;
            if(!commit(r)){return 0;}break;
        case BACKING_UP:
            if(!download_ok()||!InternalFlash_Verify(&r->active)){return 0;}
            /* 即使复位时backup_valid=1，也先明确invalid再重新复制，旧App未动。 */
            r->backup_valid=0;if(!commit(r)||!Backup_Create(&r->active,&backup)){return 0;}
            r=draft();r->backup=backup;r->backup_valid=1;if(!commit(r)){return 0;}
            /* Download可信、旧App全48KB备份双CRC可信，且INSTALLING落盘回读成功，
             * 才能在下一分支擦内部App；任何失败都不得交换或跳过此顺序。 */
            r->state=INSTALLING;if(!commit(r)){return 0;}break;
        case INSTALLING:
            if(!r->download_valid||!download_ok()||!Backup_Verify(&r->backup)){return Iap_Rollback();}
            if(!install(STORE_DOWNLOAD,32U,r->pending.size,&r->pending)){return 0;}
            r->state=PENDING_CONFIRM;r->attempts=0;if(!commit(r)){return 0;}return 1;
        case ROLLBACK:return Iap_Rollback();
        case CONFIRMED:case RECEIVING:case PENDING_CONFIRM:return 1;
        default:return 0;
        }
    }
    return 0;
}
int Iap_TrialCommit(void)
{
    Metadata *r;
    if(!loaded||current.state!=PENDING_CONFIRM||current.attempts>=3U||!InternalFlash_Verify(&current.pending)){return 0;}
    r=draft();++r->attempts;return commit(r);
}
int Iap_Confirm(void)
{
    if(!loaded||current.state!=PENDING_CONFIRM||!current.attempts||!InternalFlash_Verify(&current.pending)){return 0;}
    return commit_confirmed(&current.pending);
}

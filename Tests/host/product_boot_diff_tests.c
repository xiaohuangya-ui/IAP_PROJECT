/* 启动编排差分：完整外部调用序列、每个失败点、BKP消费及二次维护进入。
 * FSM/硬件端口为替身；实际连接测试另行保留，不能当实物验收。 */
#include "product_under_test.c"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
static jmp_buf jump_end;
static Metadata record;
static BackupSnapshot snapshot;
static unsigned fail_at,call_index,mask,decision,trace[256],count;
static int operation(unsigned tag){trace[count++]=tag;return ++call_index!=fail_at;}
uint32_t HAL_GetTick(void){return 0;}
int Recovery_Sample(RecoverySample *s){memset(s,0,sizeof(*s));return 0;}
int SpiBus_Init(void){return 0;}
int W25_Init(const W25Port *p){(void)p;return 1;}
void Uart_Service(void){}
int Uart_TxSubmit(const uint8_t *p,uint16_t n){(void)p;(void)n;return 1;}
int Uart_TxComplete(void){return 1;}
int Uart_PauseReceive(void){return 1;}
int Uart_ResumeReceive(void){return 1;}
int BootProtocol_SetControl(const IapProtocolControl *p){(void)p;return 1;}
int BootProtocol_SetReceiver(const YmPort *p){(void)p;return 1;}
void BootProtocol_Init(void){}
void BootProtocol_Poll(void){}
int Iap_Load(void){return operation(1);}
const Metadata *Iap_Record(void){return &record;}
int Iap_Recover(void){return operation(2);}
int Iap_Enter(void){return 1;}
int Iap_Begin(uint32_t n){(void)n;return 1;}
int Iap_Write(uint32_t o,const uint8_t *p,uint16_t n){(void)o;(void)p;(void)n;return 1;}
int Iap_Finish(const ImageIdentity *p){(void)p;return 1;}
void Iap_Abort(void){}
int Iap_Fail(uint32_t e){return operation(30+e);}
int Iap_TrialCommit(void){if(!operation(4))return 0;++record.attempts;return 1;}
int Iap_Confirm(void){if(!operation(5))return 0;record.active=record.pending;record.state=CONFIRMED;return 1;}
int Iap_Rollback(void){if(!operation(6))return 0;record.active=record.backup.image;record.state=CONFIRMED;return 1;}
uint32_t Iap_LogFault(void){return 0;}
void Iap_LogRecovery(void){trace[count++]=7;}
BootAction BootPolicy_Decide(const Metadata *r,const BootFacts *f){
    (void)r;trace[count++]=1000U+f->active_ok+2U*f->pending_ok+4U*f->backup_ok+8U*f->hold+16U*f->update+32U*f->confirm+64U*f->trial_mark;
    return (BootAction)decision;
}
int InternalFlash_Verify(const ImageIdentity *p){trace[count++]=40+p->version;return (mask>>(p==&record.pending?1:0))&1U;}
int Backup_Verify(const BackupIdentity *p){(void)p;trace[count++]=50;return (mask>>2)&1U;}
void Backup_ReadSnapshot(BackupSnapshot *p){trace[count++]=8;*p=snapshot;}
int Backup_BootWrite(uint32_t r,uint16_t v){if(!operation(60+r+((unsigned)v<<8)))return 0;if(r==1)snapshot.dr1=v;if(r==2)snapshot.dr2=v;if(r==3)snapshot.dr3=v;return 1;}
int Iwdg_Running(void){trace[count++]=9;return 0;}
int Iwdg_StartMeasured(void){return operation(10);}
void Iwdg_Progress(void){}
int Log_ReadNext(uint32_t *c,LogRecord *r,uint32_t *bad){(void)c;(void)r;(void)bad;return 0;}
int Jump_Prepare(const ImageIdentity *i,JumpTarget *t){(void)t;return operation(80+i->version);}
void Jump_Commit(const JumpTarget *t){(void)t;trace[count++]=90;longjmp(jump_end,1);}
static void one_boot(void){if(!setjmp(jump_end)){boot_decision();}}
int main(void){unsigned state,attempt,j,pass;
 for(state=1;state<=9;++state)for(attempt=0;attempt<=3;++attempt)
 for(mask=0;mask<256;++mask)for(decision=0;decision<=5;++decision)for(fail_at=0;fail_at<12;++fail_at){
    memset(&record,0,sizeof(record));record.state=state;record.attempts=attempt;
    record.active.version=1;record.pending.version=2;record.backup.image.version=3;record.backup_valid=(mask>>7)&1;
    hold=(mask>>3)&1;snapshot.dr1=(mask&16)?BKP_UPDATE_REQ:0;snapshot.dr2=(mask&32)?BKP_CONFIRM_REQ:0;snapshot.dr3=(mask&64)?BKP_TRIAL_MARK:0;
    call_index=count=0;
    for(pass=0;pass<2;++pass){one_boot();trace[count++]=200+record.state;trace[count++]=record.attempts;}
    /* 完整轨迹逐字节比较，不以摘要碰撞充当行为等价。 */
    j=count;fwrite(&j,sizeof(j),1,stdout);fwrite(trace,sizeof(trace[0]),count,stdout);
 }
 return 0;
}

/* Real Product_Main orchestrator; physical ports and FSM are isolated doubles.
 * Proves ordering, not real Flash/DMA/IWDG operation or full product integration. */
#include "stm32f1xx_hal.h"
#include "product_boot.h"
#include "iap_fsm.h"
#include "boot_protocol.h"
#include "backup_domain.h"
#include "recovery.h"
#include "jump.h"
#include "w25q64.h"
#include "upgrade_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
static jmp_buf end;
static IapProtocolControl control;
static Metadata record;
static unsigned mode,checks,polls,paused,resumes,prepares,jumps,trials;
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d mode=%u\n",__LINE__,mode);exit(1);}}while(0)
uint32_t HAL_GetTick(void){return 0;}
int Recovery_Sample(RecoverySample *s){memset(s,0,sizeof(*s));return 0;}
int SpiBus_Init(void){return 0;}
int W25_Init(const W25Port *p){(void)p;return 1;}
void Uart_Service(void){}
int Uart_TxSubmit(const uint8_t *p,uint16_t n){(void)p;(void)n;return 1;}
int Uart_TxComplete(void){return 1;}
int Uart_PauseReceive(void){C(!paused);paused=1;return 1;}
int Uart_ResumeReceive(void){C(paused);paused=0;++resumes;return 1;}
int BootProtocol_SetControl(const IapProtocolControl *p){control=*p;return 1;}
int BootProtocol_SetReceiver(const YmPort *p){C(p&&p->begin&&p->write&&p->finish&&p->abort);return 1;}
void BootProtocol_Init(void){}
void BootProtocol_Poll(void){
    if(polls++){longjmp(end,1);}
    C(!strcmp(control.query(CMD_IAP_VER),"BOOT UART-IAP VER=1 ACTIVE=0\r\n"));
    C(!strcmp(control.query(CMD_IAP_STATUS),"IAP STATE=7 DOWNLOAD=0 BACKUP=0 ATTEMPTS=1 ERROR=0 ACTIVE=0 PENDING=0 LOG_ERROR=0\r\n"));
    C(!strcmp(control.query(CMD_IAP_LOG),"LOG BEGIN\r\n"));
    C(!strcmp(control.next_text(),"LOG END LOG_CORRUPT=0\r\n"));C(!control.next_text());
    control.closed(1);
}
int Iap_Load(void){return 1;}
const Metadata *Iap_Record(void){return &record;}
int Iap_Recover(void){return mode!=2;}
int Iap_Enter(void){return 1;}
int Iap_Begin(uint32_t n){(void)n;return 1;}
int Iap_Write(uint32_t o,const uint8_t *p,uint16_t n){(void)o;(void)p;(void)n;return 1;}
int Iap_Finish(const ImageIdentity *p){(void)p;return 1;}
void Iap_Abort(void){}
int Iap_Fail(uint32_t e){(void)e;return 1;}
int Iap_TrialCommit(void){C(paused);++trials;return 1;}
int Iap_Confirm(void){return 1;}
int Iap_Rollback(void){return 1;}
uint32_t Iap_LogFault(void){return 0;}
void Iap_LogRecovery(void){}
BootAction BootPolicy_Decide(const Metadata *r,const BootFacts *f){C(r==&record&&f->pending_ok);return mode==4?IAP_CONFIRMED:IAP_TRIAL;}
int InternalFlash_Verify(const ImageIdentity *p){(void)p;return 1;}
int Backup_Verify(const BackupIdentity *p){(void)p;return 1;}
void Backup_ReadSnapshot(BackupSnapshot *p){memset(p,0,sizeof(*p));}
int Backup_BootWrite(uint32_t r,uint16_t v){(void)r;(void)v;return 1;}
int Iwdg_Running(void){return 0;}
int Iwdg_StartMeasured(void){return mode!=3;}
void Iwdg_Progress(void){}
int Log_ReadNext(uint32_t *c,LogRecord *r,uint32_t *bad){(void)c;(void)r;(void)bad;return 0;}
int Jump_Prepare(const ImageIdentity *i,JumpTarget *t){(void)i;(void)t;++prepares;C(paused);return mode!=1;}
void Jump_Commit(const JumpTarget *t){(void)t;C(paused&&!resumes);++jumps;longjmp(end,1);}
int main(int argc,char **argv){
    C(argc==2);mode=(unsigned)atoi(argv[1]);record.state=PENDING_CONFIRM;record.attempts=1;
    Product_EarlyInit();if(!setjmp(end)){Product_Main();}
    if(mode==0||mode==4){C(jumps==1&&prepares==1&&!resumes);C(trials==(mode==0));}
    else{C(!jumps&&resumes==1&&!paused);C(prepares==(mode==1));}
    printf("{\"result\":\"PASS\",\"mode\":%u,\"checks\":%u}\n",mode,checks);return 0;
}

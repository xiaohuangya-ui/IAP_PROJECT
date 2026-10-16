#ifndef IAP_FSM_H
#define IAP_FSM_H
#include "metadata.h"
/* 唯一前台同步owner，禁止ISR或Service/Progress重入事务API；私有草稿不外借。
 * Iap_Record只读视图到下一次FSM调用前有效，不能缓存为新的可信介质结果。 */
typedef enum {IAP_HOLD,IAP_RECOVER,IAP_CONFIRMED,IAP_TRIAL,IAP_CONFIRM,IAP_ROLLBACK} BootAction;
typedef struct {int active_ok,pending_ok,download_ok,backup_ok,hold,update,confirm,trial_mark;} BootFacts;
/* Decide纯函数；Scan/校验由调用者先完成。危险恢复优先于hold/update。 */
BootAction BootPolicy_Decide(const Metadata *record,const BootFacts *facts);
int Iap_Load(void);
const Metadata *Iap_Record(void);
int Iap_Enter(void); /* 先持久RECEIVING，成功才允许协议C */
int Iap_Begin(uint32_t file_size);
int Iap_Write(uint32_t offset,const uint8_t *data,uint16_t length);
int Iap_Finish(const ImageIdentity *image);
void Iap_Abort(void);
/* 显式副作用：有界恢复事务；失败停维护。Phase7在PENDING停止，不跳Trial。 */
int Iap_Recover(void);
int Iap_TrialCommit(void);
int Iap_Confirm(void);
int Iap_Rollback(void);
int Iap_Fail(uint32_t error); /* 明确持久FAILED；不擦内部，不伪造恢复。 */
#ifdef IAP_PHASE8_ENABLED
uint32_t Iap_LogFault(void);
void Iap_LogRecovery(void);
#endif
#endif

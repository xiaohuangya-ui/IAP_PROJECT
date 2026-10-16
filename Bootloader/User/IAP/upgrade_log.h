#ifndef UPGRADE_LOG_H
#define UPGRADE_LOG_H
#include <stdint.h>
typedef enum {LOG_START_DOWNLOAD=1,LOG_DOWNLOAD_VALID,LOG_BACKUP_VALID,LOG_INSTALL_START,
 LOG_INSTALL_OK,LOG_TRIAL_START,LOG_CONFIRM,LOG_ROLLBACK_START,LOG_ROLLBACK_OK,LOG_ERROR,LOG_POWERLOSS_RECOVERY} LogEvent;
typedef struct {uint32_t sequence,event,state,version,error,detail;} LogRecord;
/* 32B/8xLE u32：CRC最后写并回读。独立16KB日志循环，不改变Metadata决策。
 * ReadNext只读跳过损坏记录并累加corrupt；cursor=0开始，最多512槽。
 * Append失败只返回0，调用者不得撤销已成功的Journal提交。 */
int Log_ReadNext(uint32_t *cursor,LogRecord *record,uint32_t *corrupt);
int Log_Append(LogEvent event,uint32_t state,uint32_t version,uint32_t error,uint32_t detail);
#endif

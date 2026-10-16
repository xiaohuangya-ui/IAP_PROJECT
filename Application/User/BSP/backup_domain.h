#ifndef APP_BACKUP_DOMAIN_H
#define APP_BACKUP_DOMAIN_H
#include <stdint.h>
/* 简介：App只读DR3；不设置Trial、不访问W25/Metadata。 */
uint16_t Backup_AppReadTrial(void);
/* 简介：前台发出一次性请求，1写DR1=55AA，2写DR2=5AA5。
 * 返回值：1回读/权限恢复成功，否则0；调用者先确认UART硬件TC。
 * Phase0 c命令仅验证寄存器往返，不具备业务健康确认含义。 */
int Backup_AppRequest(uint32_t reg);
/* Phase9特化：仅特权线程PSP（CONTROL=2）、非ISR、屏蔽全清；
 * UpgradeCtrl唯一调用，写入/回读/权限恢复与MSP接口完全相同。 */
int Backup_AppTaskRequest(uint32_t reg);
#endif

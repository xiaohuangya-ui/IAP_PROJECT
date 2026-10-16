#ifndef BACKUP_DOMAIN_H
#define BACKUP_DOMAIN_H
#include <stdint.h>
typedef struct { uint16_t dr1, dr2, dr3; uint32_t bdcr, csr, pwr; } BackupSnapshot;
/* 简介：前台快照，只临时打开PWR/BKP时钟并恢复，不写BDCR/复位标志。 */
void Backup_ReadSnapshot(BackupSnapshot *out);
/* 简介：Boot独占DR3及请求消费。仅允许DR1/2清零、DR3零/A33A。
 * 参数：reg为1..3；返回值1写后回读且DBP恢复成功，0拒绝/失败。
 * 前台特权/MSP/IRQ未屏蔽上下文；保存并恢复时钟与DBP，绝不BDRST。 */
int Backup_BootWrite(uint32_t reg, uint16_t value);
#endif

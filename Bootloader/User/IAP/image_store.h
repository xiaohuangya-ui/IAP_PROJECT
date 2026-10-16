#ifndef IMAGE_STORE_H
#define IMAGE_STORE_H
#include "store.h"
#include "fw_header.h"
typedef struct {ImageIdentity image;uint32_t physical_crc;} BackupIdentity;
/* 只读：重新介质读取Header/完整CRC/向量；expected_file=真实会话长度。
 * 恢复时由已提交pending三元组另做一致性比较，不凭Header恢复接收完成事实。 */
int Download_Verify(uint32_t expected_file,ImageIdentity *image);
int Backup_Verify(const BackupIdentity *backup);
/* 副作用：只有FSM已提交BACKING_UP+backup_valid=0才能调用。
 * 先核实内部旧Active，再擦Backup64KB、复制完整48KB并双CRC回读。
 * 不设置valid；返回后仍需由Journal提交。 */
int Backup_Create(const ImageIdentity *active,BackupIdentity *backup);
#endif

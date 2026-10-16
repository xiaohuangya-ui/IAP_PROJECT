#ifndef IAP_METADATA_H
#define IAP_METADATA_H
#include "image_store.h"
typedef enum {CONFIRMED=1,RECEIVING,DOWNLOADED,VERIFYING,BACKING_UP,
              INSTALLING,PENDING_CONFIRM,ROLLBACK,FAILED} IapState;
typedef struct {
    uint32_t sequence,state;
    ImageIdentity active,pending;
    BackupIdentity backup;
    uint32_t download_valid,backup_valid,attempts,error;
} Metadata;
typedef struct {Metadata record;uint32_t area,slot,dirty;int found;} MetaScan;
/* 显式72B LE ABI；Scan只读。返回0=IO/冲突/非法权威，found=0为空/全脏。
 * CRC错的槽只记DIRTY；同序内容冲突拒绝。序号到FFFFFFFE停止提交。
 * Commit要求record.sequence等于刚扫描权威，拒绝旧快照；失败须重新扫描。
 * 只擦备用扇区，CRC末写+全回读后才更换权威，不声称介质原子。 */
int Meta_Valid(const Metadata *record);
void Meta_Encode(const Metadata *record,uint8_t out[72]);
int Meta_Decode(const uint8_t in[72],Metadata *record);
int Meta_Scan(MetaScan *scan);
int Meta_Commit(Metadata *record);
/* 仅隔离工装编译，空白A/B+已验证内部像；不得进入Release。 */
#ifdef IAP_PROVISION_TEST_ONLY
int Meta_Provision(const ImageIdentity *active);
#endif
#endif

#ifndef IAP_STORE_H
#define IAP_STORE_H
#include <stdint.h>
typedef enum {STORE_DOWNLOAD,STORE_BACKUP,STORE_META_A,STORE_META_B,STORE_LOG,STORE_COUNT} StoreArea;
/* Store是分区唯一权限层。偏移为分区内字节，禁止Reserved、溢出和跨区。
 * 前台同步；Erase只允许4KB或64KB边界。FSM负责先提交危险状态。
 * 不提供原始物理地址写入接口。 */
int Store_Read(StoreArea area,uint32_t offset,uint8_t *data,uint32_t length);
int Store_Program(StoreArea area,uint32_t offset,const uint8_t *data,uint32_t length);
int Store_Erase(StoreArea area,uint32_t offset,uint32_t length);
int Store_CRC(StoreArea area,uint32_t offset,uint32_t length,uint32_t *crc);
#endif

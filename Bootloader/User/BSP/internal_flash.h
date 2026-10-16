#ifndef INTERNAL_FLASH_H
#define INTERNAL_FLASH_H
#include "image_contract.h"
typedef enum { IF_OK=0, IF_RANGE, IF_ALIGN, IF_CONTEXT, IF_BUSY,
               IF_TIMEOUT, IF_LOCK, IF_PROGRAM, IF_VERIFY } FlashResult;
/* 唯一内部Flash owner。前台同步调用，禁止ISR；只准App页16..63。
 * 擦页长度必须为1024整数倍；编程地址偶数，奇数末字节补FF。
 * 源必须在RAM且调用期间有效。每次退出恢复LOCK，失败不得跳App。
 * 不操作Option Bytes；调用者负责安装事务/离线MVP授权。 */
FlashResult InternalFlash_Erase(uint32_t address,uint32_t bytes);
FlashResult InternalFlash_Program(uint32_t address,const uint8_t *data,uint32_t bytes);
int InternalFlash_Read(uint32_t address,uint8_t *data,uint32_t bytes);
int InternalFlash_Verify(const ImageIdentity *identity);
#endif

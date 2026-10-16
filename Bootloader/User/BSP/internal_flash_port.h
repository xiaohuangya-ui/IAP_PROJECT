#ifndef INTERNAL_FLASH_PORT_H
#define INTERNAL_FLASH_PORT_H
#include "internal_flash.h"
/* owner私有硬件端口；HOST链接测试替身，不进入Release。 */
int FlashPort_Context(void);
int FlashPort_Read(uint32_t address,uint8_t *data,uint32_t bytes);
/* 只读映射借用：调用者先验证App范围；同步CRC期间无Flash写入或回调。
 * HOST返回模型中的同一介质，不允许用缓存/下载副本冒充内部像。 */
const uint8_t *FlashPort_View(uint32_t address);
FlashResult FlashPort_Operation(uint32_t address,uint16_t halfword,int erase);
#endif

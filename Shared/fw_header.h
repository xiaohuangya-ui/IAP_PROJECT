#ifndef FW_HEADER_H
#define FW_HEADER_H
#include "image_contract.h"
#include "wire_codec.h"
#define FW_HEADER_SIZE 32U
/* 显式8xLE u32，available先检查；格式允许1B，启动谓词另要求8B。
 * file_size须来自Block0且恰好32+image_size，version非0/保留0/目标固定。 */
int FirmwareHeader_Decode(const uint8_t *bytes,uint32_t available,uint32_t file_size,ImageIdentity *identity);
uint32_t Firmware_ReadLE32(const uint8_t *bytes);
/* 写一个显式LE32标量；bytes覆盖4B可写普通内存，无对齐前提。 */
void Firmware_WriteLE32(uint8_t *bytes,uint32_t value);
#endif

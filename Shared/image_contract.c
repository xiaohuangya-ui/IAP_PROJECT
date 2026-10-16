#include "image_contract.h"
#include <stddef.h>
#include "wire_codec.h"
#include "byte_buffer.h"
int Bytes_AllFF(const uint8_t *buffer,uint32_t length)
{
    while(length--){if(*buffer++!=255U){return 0;}}return 1;
}
/* 无符号差值表示同一闭/开区间；size已限于APP_LIMIT，PC差值
 * 小于size也完整拒绝低于APP_BASE的地址（回绕差值远大于APP_LIMIT）。 */
int AppVector_Valid(uint32_t size, uint32_t msp, uint32_t pc)
{
    if ((uint32_t)(size - 8U) > APP_LIMIT - 8U) { return 0; }
    return (uint32_t)(msp - 0x20000001U) < 0x5000U &&
           (msp & 7U) == 0U && (pc & 1U) != 0U &&
           (uint32_t)((pc & ~1U) - APP_BASE) < size;
}
int Image_Validate(const uint8_t *image, uint32_t available,
                   const ImageIdentity *identity)
{
    if (image == NULL || identity == NULL || identity->version == 0U ||
        (uint32_t)(identity->size - 8U) > APP_LIMIT - 8U ||
        identity->size > available) { return 0; }
    if (!AppVector_Valid(identity->size, Wire_ReadLE32(image), Wire_ReadLE32(image + 4))) {
        return 0;
    }
    return CRC32_IEEE(image, identity->size) == identity->crc32;
}
int Recovery_Hold(uint32_t high_a, uint32_t high_b, uint32_t config_ok)
{
    return !(config_ok == 1U && high_a == 16U && high_b == 16U);
}

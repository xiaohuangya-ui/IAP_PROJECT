#include "crc16.h"
uint16_t CRC16_Xmodem(const uint8_t *data,uint16_t length)
{
    uint16_t crc=0U;uint32_t i;uint8_t x;
    for(i=0U;i<length;i++) {
        /* 非反射0x1021逐位递推的无表展开；截断顺序保持16位余数。 */
        x=(uint8_t)((crc>>8)^data[i]);x^=(uint8_t)(x>>4);
        crc=(uint16_t)(((uint32_t)crc<<8)^((uint32_t)x<<12)^((uint32_t)x<<5)^x);
    }
    return crc;
}

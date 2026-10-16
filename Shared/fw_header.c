#include "fw_header.h"
#include <string.h>
uint32_t Firmware_ReadLE32(const uint8_t *p){return Wire_ReadLE32(p);}
void Firmware_WriteLE32(uint8_t *p,uint32_t value){Wire_WriteLE32(p,value);}
int FirmwareHeader_Decode(const uint8_t *p,uint32_t available,uint32_t file_size,ImageIdentity *id)
{
    uint32_t size;
    if(!p || !id || available<32U){return 0;}
    size=Firmware_ReadLE32(p+12);
    if(Firmware_ReadLE32(p)!=0x46574D47U || Firmware_ReadLE32(p+4)!=1U ||
       !Firmware_ReadLE32(p+8) || (uint32_t)(size-1U)>=APP_LIMIT || file_size!=size+32U ||
       Firmware_ReadLE32(p+20)!=APP_BASE || Firmware_ReadLE32(p+24)!=0U ||
       CRC32_IEEE(p,28U)!=Firmware_ReadLE32(p+28)){return 0;}
    id->version=Firmware_ReadLE32(p+8);id->size=size;id->crc32=Firmware_ReadLE32(p+16);return 1;
}

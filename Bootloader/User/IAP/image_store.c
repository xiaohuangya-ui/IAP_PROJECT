#include "image_store.h"
#include "internal_flash.h"
static int vectors(StoreArea area,uint32_t offset,uint32_t size)
{
    uint8_t b[8];return Store_Read(area,offset,b,8U)&&
        AppVector_Valid(size,Wire_ReadLE32(b),Wire_ReadLE32(b+4));
}
int Download_Verify(uint32_t total,ImageIdentity *id)
{
    uint8_t b[32];uint32_t crc;
    return id && Store_Read(STORE_DOWNLOAD,0,b,32U) &&
        FirmwareHeader_Decode(b,32U,total,id) && id->size>=8U &&
        Store_CRC(STORE_DOWNLOAD,32U,id->size,&crc) && crc==id->crc32 &&
        vectors(STORE_DOWNLOAD,32U,id->size);
}
int Backup_Verify(const BackupIdentity *b)
{
    uint32_t crc;
    return b && b->image.version && b->image.size>=8U && b->image.size<=APP_LIMIT &&
        Store_CRC(STORE_BACKUP,0,APP_LIMIT,&crc) && crc==b->physical_crc &&
        Store_CRC(STORE_BACKUP,0,b->image.size,&crc) && crc==b->image.crc32 &&
        vectors(STORE_BACKUP,0,b->image.size);
}
int Backup_Create(const ImageIdentity *active,BackupIdentity *out)
{
    uint8_t b[256];uint32_t off,crc=0xFFFFFFFFU;BackupIdentity candidate;
    if(!out||!InternalFlash_Verify(active)||!Store_Erase(STORE_BACKUP,0,65536U)){return 0;}
    for(off=0;off<APP_LIMIT;off+=sizeof(b)){
        if(!InternalFlash_Read(APP_BASE+off,b,sizeof(b))||
           !Store_Program(STORE_BACKUP,off,b,sizeof(b))){return 0;}
        crc=CRC32_Update(crc,b,sizeof(b));
    }
    candidate.image=*active;candidate.physical_crc=crc^0xFFFFFFFFU;
    if(!Backup_Verify(&candidate)||!InternalFlash_Verify(active)){return 0;}
    *out=candidate;return 1;
}

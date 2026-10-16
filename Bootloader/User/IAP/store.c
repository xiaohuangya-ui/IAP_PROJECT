#include "store.h"
#include "w25q64.h"
#include "image_contract.h"
/* 固定产品分区，以4KB为单位；range拒绝非法枚举后才允许索引。 */
static const uint8_t bases[]={0,16,32,33,34};
static const uint8_t sizes[]={16,16,1,1,4};
static int range(StoreArea area,uint32_t off,uint32_t n)
{uint32_t size;if((unsigned)area>=STORE_COUNT||!n){return 0;}
 size=(uint32_t)sizes[area]<<12;return off<size&&n<=size-off;}
int Store_Read(StoreArea a,uint32_t o,uint8_t *p,uint32_t n)
{return p && range(a,o,n) && W25_Read(((uint32_t)bases[a]<<12)+o,p,n);}
int Store_Program(StoreArea a,uint32_t o,const uint8_t *p,uint32_t n)
{return p && range(a,o,n) && W25_Write(((uint32_t)bases[a]<<12)+o,p,n);}
int Store_Erase(StoreArea a,uint32_t o,uint32_t n)
{return range(a,o,n) && W25_Erase(((uint32_t)bases[a]<<12)+o,n);}
int Store_CRC(StoreArea a,uint32_t o,uint32_t n,uint32_t *out)
{
    uint8_t b[128];uint32_t k,crc=0xFFFFFFFFU;
    if(!out||!range(a,o,n)){return 0;}
    while(n){k=n>sizeof(b)?sizeof(b):n;if(!Store_Read(a,o,b,k)){return 0;}
        crc=CRC32_Update(crc,b,k);o+=k;n-=k;}
    *out=crc^0xFFFFFFFFU;return 1;
}

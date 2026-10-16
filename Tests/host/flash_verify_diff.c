#include "internal_flash_port.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static uint8_t flash[APP_LIMIT];static uint32_t random_state=17U;
static uint32_t rnd(void){random_state=random_state*1664525U+1013904223U;return random_state;}
int Before_InternalFlash_Verify(const ImageIdentity *id);
int FlashPort_Context(void){return 1;}
int FlashPort_Read(uint32_t a,uint8_t *p,uint32_t n)
{assert(a>=APP_BASE&&a-APP_BASE<APP_LIMIT&&n<=APP_LIMIT-(a-APP_BASE));memcpy(p,flash+a-APP_BASE,n);return 1;}
const uint8_t *FlashPort_View(uint32_t a){assert(a==APP_BASE);return flash;}
FlashResult FlashPort_Operation(uint32_t a,uint16_t v,int erase)
{(void)a;(void)v;(void)erase;assert(0);return IF_CONTEXT;}
static void put(uint8_t *p,uint32_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);p[2]=(uint8_t)(x>>16);p[3]=(uint8_t)(x>>24);}
static unsigned cases;
static void same(ImageIdentity *id,int expected)
{int before=Before_InternalFlash_Verify(id),after=InternalFlash_Verify(id);assert(before==after&&after==expected);++cases;}
int main(void)
{
    unsigned i,j;ImageIdentity id;
    same(0,0);
    for(i=0;i<400U;++i){
        uint32_t size=i<3U?(i==0?8U:i==1?9U:APP_LIMIT):8U+rnd()%(APP_LIMIT-7U);
        for(j=0;j<size;++j){flash[j]=(uint8_t)(rnd()>>24);}
        put(flash,0x20005000U);put(flash+4,APP_BASE+7U);
        id.version=1;id.size=size;id.crc32=CRC32_IEEE(flash,size);same(&id,1);
        flash[size-1]^=1;same(&id,0);flash[size-1]^=1;
        ++id.crc32;same(&id,0);--id.crc32;
        put(flash,0x20000000U);id.crc32=CRC32_IEEE(flash,size);same(&id,0);
        put(flash,0x20005000U);put(flash+4,APP_BASE+size+1U);id.crc32=CRC32_IEEE(flash,size);same(&id,0);
        id.version=0;same(&id,0);id.version=1;id.size=7;same(&id,0);
        id.size=APP_LIMIT+1U;same(&id,0);id.size=0xFFFFFFFFU;same(&id,0);
    }
    printf("{\"PASS\":true,\"differential_cases\":%u,\"destructive_calls\":0}\n",cases);return 0;
}

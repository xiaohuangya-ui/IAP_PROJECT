#include "internal_flash_port.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t memory[65536];static unsigned calls,fail_at,checks;static int context=1;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL line %d\n",__LINE__);exit(1);}}while(0)
int FlashPort_Context(void){return context;}
const uint8_t *FlashPort_View(uint32_t a)
{CHECK(a==APP_BASE);return memory+a-0x08000000U;}
int FlashPort_Read(uint32_t a,uint8_t *p,uint32_t n)
{CHECK(a>=APP_BASE && n<=0x08010000U-a);memcpy(p,memory+a-0x08000000U,n);return 1;}
FlashResult FlashPort_Operation(uint32_t a,uint16_t v,int erase)
{
    CHECK(a>=APP_BASE && a<0x08010000U);++calls;
    if(calls==fail_at){return IF_TIMEOUT;}
    if(erase){memset(memory+a-0x08000000U,255,1024);}
    else{memory[a-0x08000000U]=(uint8_t)v;memory[a-0x08000000U+1]=(uint8_t)(v>>8);}
    return IF_OK;
}
int main(void)
{
    uint8_t payload[1025];ImageIdentity id;unsigned i;
    memset(memory,0xA5,sizeof(memory));memset(payload,0x12,sizeof(payload));
    CHECK(InternalFlash_Erase(0x08000000U,1024)==IF_RANGE);
    CHECK(InternalFlash_Erase(APP_BASE-1,1024)==IF_RANGE);
    CHECK(InternalFlash_Erase(APP_BASE,0)==IF_RANGE);
    CHECK(InternalFlash_Erase(APP_BASE,0xFFFFFFFFU)==IF_RANGE);
    CHECK(InternalFlash_Erase(APP_BASE+1,1024)==IF_ALIGN);
    CHECK(InternalFlash_Erase(APP_BASE,1023)==IF_ALIGN);
    CHECK(InternalFlash_Program(0x0800FFFFU,payload,1)==IF_RANGE);
    CHECK(InternalFlash_Program(APP_BASE+1,payload,2)==IF_ALIGN);
    CHECK(InternalFlash_Program(APP_BASE,0,2)==IF_RANGE);
    CHECK(calls==0);context=0;
    CHECK(InternalFlash_Erase(APP_BASE,1024)==IF_CONTEXT);context=1;
    CHECK(InternalFlash_Program(APP_BASE,payload,2)==IF_PROGRAM);
    CHECK(InternalFlash_Erase(APP_BASE,2048)==IF_OK);
    CHECK(InternalFlash_Program(APP_BASE,payload,1025)==IF_OK);
    CHECK(memcmp(memory+0x4000,payload,1025)==0 && memory[0x4401]==255);
    CHECK(InternalFlash_Program(APP_BASE,payload,2)==IF_PROGRAM);
    CHECK(InternalFlash_Erase(0x0800FC00U,1024)==IF_OK);
    CHECK(InternalFlash_Program(0x0800FFFEU,payload,2)==IF_OK);
    CHECK(InternalFlash_Erase(APP_BASE,1024)==IF_OK);
    fail_at=calls+2;CHECK(InternalFlash_Program(APP_BASE,payload,8)==IF_TIMEOUT);
    CHECK(memory[0x4000]==0x12 && memory[0x4002]==255);fail_at=0;
    CHECK(InternalFlash_Erase(APP_BASE,1024)==IF_OK);
    payload[0]=0;payload[1]=0x50;payload[2]=0;payload[3]=0x20;
    payload[4]=9;payload[5]=0x40;payload[6]=0;payload[7]=8;
    id.version=0x10000;id.size=9;id.crc32=CRC32_IEEE(payload,9);
    CHECK(InternalFlash_Program(APP_BASE,payload,9)==IF_OK);
    CHECK(InternalFlash_Verify(&id));memory[0x4008]^=1;CHECK(!InternalFlash_Verify(&id));
    id.size=0xFFFFFFFFU;CHECK(!InternalFlash_Verify(&id));
    for(i=0;i<0x4000;++i){CHECK(memory[i]==0xA5);}
    printf("{\"checks\":%u,\"result\":\"PASS\",\"scope\":\"actual owner C, mock physical port\"}\n",checks);
    return 0;
}

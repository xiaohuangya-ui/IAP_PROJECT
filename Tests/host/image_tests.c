#include "image_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned assertions;
#define CHECK(x) do { ++assertions; if (!(x)) { fprintf(stderr,"FAIL line %u\n",(unsigned)__LINE__); return 1; } } while(0)
static void le32(uint8_t *p, uint32_t value)
{
    unsigned i;
    for (i=0;i<4;++i) { p[i]=(uint8_t)(value>>(8*i)); }
}
int main(int argc, char **argv)
{
    static uint8_t data[49152];
    static const uint32_t sizes[]={0,1,7,8,9,0xBFFF,0xC000,0xC001,0xFFFFFFFF};
    static const uint32_t stacks[]={0,0x20000000,0x20000004,0x20000008,0x20004FFC,0x20005000,0x20005008,0xFFFFFFFF};
    static const uint32_t pcs[]={0,0x08003FFF,0x08004000,0x08004001,0x08004007,0x08004009,0x0800FFFF,0x08010001,0xFFFFFFFF};
    uint32_t size, expected_crc, state, at, chunk, a,b,c,i,j,k,sp,pc;
    FILE *stream;
    ImageIdentity id;
    CHECK(argc==2);
    stream=fopen(argv[1],"rb"); CHECK(stream!=NULL);
    CHECK(CRC32_IEEE((const uint8_t *)"123456789",9)==0xCBF43926U);
    CHECK(CRC32_IEEE(NULL,0)==0);
    CHECK(CRC32_Update(0x12345678U,NULL,0)==0x12345678U);
    while (fread(&size,4,1,stream)==1) {
        CHECK(fread(&expected_crc,4,1,stream)==1 && size<=sizeof(data));
        CHECK(fread(data,1,size,stream)==size);
        CHECK(CRC32_IEEE(data,size)==expected_crc);
        state=0xFFFFFFFFU; at=0;
        while (at<size) {
            chunk=(at*37U+17U)%997U+1U;
            if (chunk>size-at) { chunk=size-at; }
            state=CRC32_Update(state,data+at,chunk); at+=chunk;
        }
        CHECK((state^0xFFFFFFFFU)==expected_crc);
    }
    CHECK(feof(stream)!=0); fclose(stream);
    for(a=0;a<18;++a) for(b=0;b<18;++b) for(c=0;c<3;++c) {
        CHECK(Recovery_Hold(a,b,c)==!(c==1 && a==16 && b==16));
    }
    for(i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i)
    for(j=0;j<sizeof(stacks)/sizeof(stacks[0]);++j)
    for(k=0;k<sizeof(pcs)/sizeof(pcs[0]);++k) {
        int expected;
        size=sizes[i]; sp=stacks[j]; pc=pcs[k];
        expected=size>=8 && size<=49152 && sp>0x20000000 && sp<=0x20005000 &&
                 sp%8==0 && (pc&1)!=0 && (pc&~1U)>=APP_BASE && (pc&~1U)-APP_BASE<size;
        CHECK(AppVector_Valid(size,sp,pc)==expected);
    }
    memset(data,0xA5,sizeof(data)); le32(data,0x20005000); le32(data+4,APP_BASE+1);
    for(i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        size=sizes[i]; id.version=1; id.size=size; id.crc32=0;
        if (size<8 || size>49152) { CHECK(Image_Validate(data,1,&id)==0); continue; }
        id.crc32=CRC32_IEEE(data,size);
        CHECK(Image_Validate(data,size,&id)==1);
        CHECK(Image_Validate(data,size-1,&id)==0);
        data[size-1]^=1; CHECK(Image_Validate(data,size,&id)==0); data[size-1]^=1;
        id.version=0; CHECK(Image_Validate(data,size,&id)==0);
    }
    id.version=1; id.size=8;
    for(j=0;j<sizeof(stacks)/sizeof(stacks[0]);++j)
    for(k=0;k<sizeof(pcs)/sizeof(pcs[0]);++k) {
        le32(data,stacks[j]); le32(data+4,pcs[k]); id.crc32=CRC32_IEEE(data,8);
        CHECK(Image_Validate(data,8,&id)==AppVector_Valid(8,stacks[j],pcs[k]));
    }
    CHECK(Image_Validate(NULL,49152,&id)==0);
    CHECK(Image_Validate(data,49152,NULL)==0);
    printf("{\"result\":\"PASS\",\"assertions\":%u}\n",assertions);
    return 0;
}

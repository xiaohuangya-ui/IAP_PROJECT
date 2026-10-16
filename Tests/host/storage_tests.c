#include "w25q64.h"
#include "image_store.h"
#include "internal_flash_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t nor[0x800000],flash[0xC000],sr,protected_sr;
static uint32_t tick,checks,writes,erases,progresses;static int no_wel,stuck,io_fail,corrupt;
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d\n",__LINE__);exit(1);}}while(0)
int SpiBus_Transfer(const uint8_t *tx,uint8_t *rx,uint16_t n)
{
    uint32_t a,i,size;memset(rx,0,n);if(io_fail){return 0;}
    switch(tx[0]){
    case 0x9F:C(n==4);rx[1]=0xEF;rx[2]=0x40;rx[3]=0x17;break;
    case 5:rx[1]=(uint8_t)(sr|protected_sr|(stuck?1:0));break;
    case 0x35:break;
    case 6:if(!no_wel){sr=2;}break;
    case 3:case 2:case 0x20:case 0xD8:
        C(n>=4);a=((uint32_t)tx[1]<<16)|((uint32_t)tx[2]<<8)|tx[3];C(a<sizeof(nor));
        if(tx[0]==3){C(a+n-4<=sizeof(nor));memcpy(rx+4,nor+a,n-4);if(corrupt){rx[4]^=1;}}
        else{
            C(sr==2);sr=0;
            if(tx[0]==2){C(n<=260 && (a&255U)+n-4<=256);++writes;
                for(i=4;i<n;++i){nor[a+i-4]&=tx[i];}}
            else{size=tx[0]==0x20?4096:65536;C((a&(size-1))==0);++erases;memset(nor+a,255,size);}
        }break;
    default:C(0);
    }return 1;
}
int SpiBus_Exchange(const uint8_t *tx,uint8_t *rx,uint16_t n)
{return n<=4 && SpiBus_Transfer(tx,rx,n);}
static uint32_t clock_ms(void){return tick;}
static void service(void){++tick;}
static void progress(void){++progresses;}
int FlashPort_Context(void){return 1;}
const uint8_t *FlashPort_View(uint32_t a){return flash+a-APP_BASE;}
int FlashPort_Read(uint32_t a,uint8_t *p,uint32_t n)
{C(a>=APP_BASE && n<=APP_LIMIT-(a-APP_BASE));memcpy(p,flash+a-APP_BASE,n);return 1;}
FlashResult FlashPort_Operation(uint32_t a,uint16_t v,int e)
{(void)a;(void)v;(void)e;C(0);return IF_PROGRAM;}
int main(void)
{
    W25Port p={clock_ms,service,progress};uint8_t b[513],read[513];unsigned i,n;BackupIdentity backup;ImageIdentity active;
    memset(nor,255,sizeof(nor));memset(b,0xA6,sizeof(b));C(W25_Init(&p));
    C(!W25_Write(0x800000,b,1));C(!W25_Write(0x7FFFFF,b,2));C(!W25_Write(0,b,0xFFFFFFFFU));
    C(!W25_PageProgram(255,b,2));C(!W25_Erase(1,4096));C(!W25_Erase(0,32768));
    C(!Store_Program((StoreArea)-1,0,b,1));C(!Store_Program(STORE_META_A,4095,b,2));
    C(!Store_Erase(STORE_META_A,0,65536));C(!Store_Program(STORE_LOG,16384,b,1));C(writes==0&&erases==0);
    C(Store_Program(STORE_DOWNLOAD,255,b,513));C(writes==3);C(Store_Read(STORE_DOWNLOAD,255,read,513));C(!memcmp(b,read,513));
    C(!Store_Program(STORE_DOWNLOAD,255,b,1));C(Store_Erase(STORE_DOWNLOAD,0,4096));
    no_wel=1;n=writes;C(!Store_Program(STORE_DOWNLOAD,0,b,1));C(writes==n);no_wel=0;C(W25_Init(&p));
    protected_sr=4;C(!Store_Erase(STORE_DOWNLOAD,0,4096));protected_sr=0;C(W25_Init(&p));
    tick=0xFFFFFFFEU;stuck=1;C(!Store_Read(STORE_DOWNLOAD,0,b,1));C(tick<20);stuck=0;C(W25_Init(&p));
    io_fail=1;C(!Store_Read(STORE_DOWNLOAD,0,b,1));io_fail=0;C(W25_Init(&p));
    for(i=0;i<sizeof(flash);++i){flash[i]=(uint8_t)(i*17U+7U);}
    flash[0]=0;flash[1]=0x50;flash[2]=0;flash[3]=0x20;flash[4]=9;flash[5]=0x40;flash[6]=0;flash[7]=8;
    active.version=0x10000;active.size=777;active.crc32=CRC32_IEEE(flash,active.size);
    C(Backup_Create(&active,&backup));C(!memcmp(nor+0x10000,flash,sizeof(flash)));
    C(backup.physical_crc==CRC32_IEEE(flash,sizeof(flash)));C(Backup_Verify(&backup));
    nor[0x10000+40000]^=1;C(!Backup_Verify(&backup));nor[0x10000+40000]^=1;
    nor[0x10000+50]^=1;C(!Backup_Verify(&backup));nor[0x10000+50]^=1;
    C(!Store_Read(STORE_DOWNLOAD,65535,b,2));C(!Store_Read(STORE_DOWNLOAD,0,b,0));
    for(i=0x20000;i<sizeof(nor);++i){C(nor[i]==255);}
    C(progresses>0);
    printf("{\"checks\":%lu,\"result\":\"PASS\",\"physical_writes\":%lu,\"scope\":\"actual W25/Store/Backup with SPI medium mock\"}\n",(unsigned long)checks,(unsigned long)writes);
    return 0;
}

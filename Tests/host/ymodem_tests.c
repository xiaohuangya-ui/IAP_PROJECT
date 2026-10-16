#include "ymodem.h"
#include "crc16.h"
#include "router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,writes,finishes,aborts,begins,write_fail,finish_fail,begin_fail,tx_busy;
static uint32_t tick,declared,saved,write_ms;
static uint8_t storage[49184],file[49184],output[4096];
static unsigned outn;
static YModem y;
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"line %d %s (state=%d error=%d)\n",__LINE__,#x,y.state,y.error);exit(1);}}while(0)
static void le(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static int begin(void *ctx,uint32_t n){(void)ctx;begins++;declared=n;saved=0;return !begin_fail;}
static int write_data(void *ctx,uint32_t off,const uint8_t *p,uint16_t n)
{(void)ctx;C(off==saved && n<=declared-saved);writes++;tick+=write_ms;if(write_fail){return 0;}memcpy(storage+off,p,n);saved+=n;return 1;}
static int finish(void *ctx,const ImageIdentity *id)
{(void)ctx;finishes++;C(saved==declared);return !finish_fail && Image_Validate(storage+32,saved-32,id);}
static void abort_data(void *ctx){(void)ctx;aborts++;}
static int tx(void *ctx,const uint8_t *p,uint16_t n)
{(void)ctx;if(tx_busy){return 0;}C(n<=2 && outn+n<=sizeof(output));memcpy(output+outn,p,n);outn+=n;return 1;}
static uint32_t clock_ms(void *ctx){(void)ctx;return tick;}
static const YmPort port={0,begin,write_data,finish,abort_data,tx,clock_ms};
static void start(void)
{writes=finishes=aborts=begins=write_fail=finish_fail=begin_fail=tx_busy=0;saved=outn=write_ms=0;tick=0;C(YModem_Start(&y,&port));YModem_Poll(&y);C(outn==1 && output[0]=='C');outn=0;}
static void byte(uint8_t b){YModem_Feed(&y,b);}
static void response(uint8_t a,uint8_t b)
{YModem_Poll(&y);C(outn==(b?2U:1U));C(output[0]==a);if(b){C(output[1]==b);}outn=0;}
static void pkt(uint8_t id,const uint8_t *p,uint16_t n,int bad)
{
    uint16_t i,crc=CRC16_Xmodem(p,n);C(YModem_CanRead(&y));byte(n==128?1:2);byte(id);byte((uint8_t)~id);
    for(i=0;i<n;i++){byte(p[i]);}byte((uint8_t)(crc>>8));byte((uint8_t)(crc^(bad?1U:0U)));
}
static uint32_t make_file(uint32_t size)
{
    uint32_t i;for(i=0;i<size;i++){file[32+i]=(uint8_t)(i*31U+7U);}
    le(file+32,0x20005000);le(file+36,0x08004009);
    le(file,0x46574D47);le(file+4,1);le(file+8,0x10000);le(file+12,size);le(file+16,CRC32_IEEE(file+32,size));le(file+20,APP_BASE);le(file+24,0);le(file+28,CRC32_IEEE(file,28));return size+32;
}
static void block0(uint32_t size)
{
    uint8_t p[128]={0};int n;memcpy(p,"app.fw",6);n=snprintf((char *)p+7,121,"%lu",(unsigned long)size);C(n>0);pkt(0,p,128,0);
}
static void data(uint8_t id,uint32_t off,uint16_t n,uint32_t total,int bad)
{
    uint8_t p[1024];uint32_t count=total-off<n?total-off:n;memset(p,0x1A,n);memcpy(p,file+off,count);pkt(id,p,n,bad);
}
static void complete(void)
{
    uint8_t empty[128]={0};byte(4);response(21,0);C(y.state==YM_EOT2);byte(4);response(6,'C');C(y.state==YM_FINAL);
    byte(4);response(6,'C');pkt(0,empty,128,0);response(6,0);C(y.state==YM_DONE && finishes==1);
    tick+=1999;pkt(0,empty,128,0);response(6,0);C(finishes==1);tick++;YModem_Poll(&y);C(y.closed);
}
int main(int argc,char **argv)
{
    uint32_t total,off;uint8_t id,p[1024];unsigned i,k;ImageIdentity identity;
    C(CRC16_Xmodem((const uint8_t *)"123456789",9)==0x31C3);C(CRC16_Xmodem(NULL,0)==0);
    C(CRC32_IEEE((const uint8_t *)"123456789",9)==0xCBF43926);C(CRC32_IEEE(NULL,0)==0);
    total=make_file(1001);C(FirmwareHeader_Decode(file,32,total,&identity));
    for(i=0;i<32;i++){C(!FirmwareHeader_Decode(file,i,total,&identity));}
    for(i=0;i<32;i++){file[i]^=1;C(!FirmwareHeader_Decode(file,32,total,&identity));file[i]^=1;}
    C(!FirmwareHeader_Decode(file,32,total+1,&identity));
    /* 字段语义负例先重算Header CRC，避免只覆盖CRC失败分支。 */
    {static const uint32_t offsets[]={0,4,8,12,12,20,24};
     static const uint32_t values[]={0,2,0,0,49153,0x08000000,1};
     for(i=0;i<7;i++){memcpy(p,file,32);le(p+offsets[i],values[i]);le(p+28,CRC32_IEEE(p,28));C(!FirmwareHeader_Decode(p,32,total,&identity));}}
    C(argc==3);
    {FILE *f=fopen(argv[1],"rb");uint8_t h[8];C(f!=NULL);
     while(fread(h,1,8,f)==8){uint16_t n=(uint16_t)(h[0]|(uint16_t)h[1]<<8);C(n<=1024);C(fread(p,1,n,f)==n);C(CRC16_Xmodem(p,n)==(uint16_t)(h[2]|(uint16_t)h[3]<<8));C(CRC32_IEEE(p,n)==Firmware_ReadLE32(h+4));}C(feof(f));fclose(f);}
    {FILE *f=fopen(argv[2],"rb");C(f!=NULL);C(fread(p,1,96,f)==96);fclose(f);C(FirmwareHeader_Decode(p,96,96,&identity));C(Image_Validate(p+32,64,&identity));}
    /* 正常SOH/STX、任意字节Feed、尾填充裁剪、重复包/Block0、丢EOT/尾ACK。 */
    start();block0(total);response(6,'C');block0(total);response(6,'C');C(begins==1);
    data(1,0,1024,total,1);response(21,0);C(saved==0 && writes==0);
    data(2,0,1024,total,0);response(21,0);C(saved==0);
    write_ms=1500;data(1,0,1024,total,0);response(6,0);C(saved==1024);YModem_Poll(&y);C(outn==0);
    data(1,0,1024,total,0);response(6,0);C(saved==1024 && writes==1);
    data(2,1024,128,total,0);response(6,0);C(saved==total && !memcmp(file,storage,total));complete();
    /* 48KB上界、128B块序号255→0→1回绕，真正独立接收存储和回读验证。 */
    total=make_file(49152);start();block0(total);response(6,'C');off=0;id=1;
    while(off<total){data(id,off,128,total,0);response(6,0);off+=total-off<128?total-off:128;id++;}
    C(writes==385 && saved==49184 && !memcmp(storage,file,total));complete();
    /* 非空尾Block0拒绝批次第二文件；提前EOT、Header坏、镜像坏、sink失败。 */
    total=make_file(64);start();block0(total);response(6,'C');byte(4);response(24,24);C(y.error==YM_ERR_LENGTH && aborts==1);
    start();block0(total+1);response(6,'C');data(1,0,128,total,0);response(24,24);C(y.error==YM_ERR_HEADER && writes==0);
    start();block0(total);response(6,'C');file[40]^=1;data(1,0,128,total,0);response(6,0);byte(4);response(24,24);C(y.error==YM_ERR_IMAGE);file[40]^=1;
    start();begin_fail=1;block0(total);response(24,24);C(y.error==YM_ERR_SINK && saved==0 && !aborts);
    start();block0(total);response(6,'C');write_fail=1;data(1,0,128,total,0);response(24,24);C(y.error==YM_ERR_SINK && saved==0 && aborts==1);
    start();block0(total);response(6,'C');data(1,0,128,total,0);response(6,0);byte(4);response(21,0);byte(4);response(6,'C');memset(p,0,128);p[0]='x';pkt(0,p,128,0);response(24,24);C(y.error==YM_ERR_LENGTH && finishes==0);
    start();block0(total);response(6,'C');data(1,0,128,total,0);response(6,0);byte(4);response(21,0);byte(4);response(6,'C');memset(p,0,128);finish_fail=1;pkt(0,p,128,0);response(24,24);C(y.error==YM_ERR_SINK && aborts==1);
    /* Block0长度/十进制/文件名负例，重包内容必须逐字节相同。 */
    for(i=0;i<6;i++){start();memset(p,0,128);strcpy((char *)p,"app.fw");strcpy((char *)p+7,(const char*[]){"32","49185","-40","999999999999","40x",""}[i]);pkt(0,p,128,0);response(24,24);C(!begins);}
    start();block0(total);response(6,'C');data(1,0,128,total,0);response(6,0);k=writes;file[50]^=1;data(1,0,128,total,0);response(21,0);C(writes==k);file[50]^=1;
    /* 部分帧超时→重传；启动30s、十次连续坏包、双CAN、UART断会话。 */
    start();block0(total);response(6,'C');byte(1);byte(1);tick+=1000;YModem_Poll(&y);response(21,0);data(1,0,128,total,0);response(6,0);
    start();for(i=0;i<29;i++){tick+=1000;YModem_Poll(&y);response('C',0);}tick+=1000;YModem_Poll(&y);response(24,24);C(y.error==YM_ERR_TIMEOUT);
    start();block0(total);response(6,'C');for(i=0;i<10;i++){memset(p,0,128);pkt(1,p,128,1);response(i==9?24:21,i==9?24:0);}C(y.state==YM_ERROR && aborts==1);
    start();byte(24);C(y.state==YM_BLOCK0);byte(24);response(24,24);C(y.state==YM_CANCELLED);
    start();block0(total);response(6,'C');YModem_UartFault(&y);response(24,24);C(y.error==YM_ERR_UART && aborts==1);
    start();memset(p,0,128);pkt(0,p,128,1);response(21,0);C(!begins);
    start();memset(p,0,128);memcpy(p,"app.fw\0" "000000096 0 0",20);pkt(0,p,128,0);response(6,'C');C(y.file_size==96 && begins==1);YModem_UartFault(&y);response(24,24);
    start();byte(1);byte(0);byte(0);memset(p,0,128);
    for(i=0;i<130;i++){byte(0);}response(21,0);C(!begins && y.state==YM_BLOCK0);
    /* 合法Header但不可启动向量、超声明数据、新包跳号、重复包原样重试。 */
    total=make_file(64);le(file+32,0x20004FFC);le(file+16,CRC32_IEEE(file+32,64));le(file+28,CRC32_IEEE(file,28));
    start();block0(total);response(6,'C');data(1,0,128,total,0);response(6,0);byte(4);response(24,24);C(y.error==YM_ERR_IMAGE);
    total=make_file(64);start();block0(total);response(6,'C');data(1,0,128,total,0);response(6,0);memset(p,0,128);pkt(2,p,128,0);response(21,0);C(writes==1 && saved==total);
    /* TX忙不提前消费、1000ms失败关闭；tick回绕不会提前超时。 */
    start();tx_busy=1;block0(total);C(!YModem_CanRead(&y));tick+=999;YModem_Poll(&y);C(y.state==YM_DATA);tick++;YModem_Poll(&y);C(y.error==YM_ERR_TX && aborts==1);
    start();tick=0xFFFFFFF0;y.wait_since=tick;y.start=tick;tick+=999;YModem_Poll(&y);C(outn==0 && y.state==YM_BLOCK0);tick++;YModem_Poll(&y);response('C',0);
    C(!YModem_Start(NULL,&port));
    printf("{\"assertions\":%u,\"result\":\"PASS\",\"max_file\":49184,\"storage\":\"HOST RAM ONLY\"}\n",checks);return 0;
}

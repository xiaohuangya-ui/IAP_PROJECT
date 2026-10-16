/* 仅PC进程桥：真实BootProtocol/Router/YModem + 内存sink/虚拟毫秒/UART mock。
 * 不进Keil/Release，不引用serial/HAL/寄存器/Flash/NOR。stdin十六进制代表
 * Python Sender实际发出的字节；stdout返回真实协议代码产生的回复。
 * sink独立保存有效文件并在finish回读Image_Validate；Q返回内存供Python逐字节比对。
 */
#include "boot_protocol.h"
#include "uart_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t input[4096],output[1024],storage[49184];
static unsigned head,tail,outn,writes,finishes,aborts,fences,begins;
static uint32_t tick,declared,saved;
static void check(int ok){if(!ok){fprintf(stderr,"Bridge invariant failed\n");exit(2);}}
int Uart_Start(void){return 1;}
void Uart_Service(void){}
uint32_t Uart_TakeErrors(void){return 0U;}
int Uart_TxBusy(void){return 0;}
int Uart_TxComplete(void){return 1;}
int Uart_ReadByte(uint8_t *b){if(tail==head){return 0;}*b=input[tail++];return 1;}
int Uart_RxFence(void){tail=head;fences++;return 1;}
int Uart_TxSubmit(const uint8_t *p,uint16_t n)
{check(n<=sizeof(output)-outn);memcpy(output+outn,p,n);outn+=n;return 1;}
static int begin(void *context,uint32_t n)
{(void)context;check(n<=sizeof(storage));declared=n;saved=0U;begins++;return 1;}
static int write_data(void *context,uint32_t offset,const uint8_t *p,uint16_t n)
{(void)context;check(offset==saved && n<=declared-saved);memcpy(storage+saved,p,n);saved+=n;writes++;return 1;}
static int finish(void *context,const ImageIdentity *id)
{(void)context;finishes++;return saved==declared && saved>=40U && Image_Validate(storage+32U,saved-32U,id);}
static void abort_data(void *context){(void)context;aborts++;}
static int tx(void *context,const uint8_t *p,uint16_t n)
{(void)context;return Uart_TxSubmit(p,n);}
static uint32_t clock_ms(void *context){(void)context;return tick;}
static unsigned nibble(char c)
{if(c>='0' && c<='9'){return (unsigned)(c-'0');}if(c>='a' && c<='f'){return (unsigned)(c-'a'+10);}check(0);return 0;}
static void pump(void){unsigned i;for(i=0;i<4096U;i++){BootProtocol_Poll();}}
static void response(void)
{unsigned i;for(i=0;i<outn;i++){printf("%02x",output[i]);}putchar('\n');fflush(stdout);outn=0U;}
int main(void)
{
    char line[2200];size_t n,i;
    const YmPort port={0,begin,write_data,finish,abort_data,tx,clock_ms};
    check(BootProtocol_SetReceiver(&port));BootProtocol_Init();
    while(fgets(line,sizeof(line),stdin)) {
        n=strcspn(line,"\r\n");line[n]='\0';
        if(line[0]=='W') {
            check(n>1U && (n-1U)%2U==0U && (n-1U)/2U<=sizeof(input));
            head=tail=0U;
            for(i=1U;i<n;i+=2U){input[head++]=(uint8_t)(nibble(line[i])*16U+nibble(line[i+1U]));}
            tick+=1U;pump();check(tail==head);response();
        } else if(line[0]=='T') {tick+=(uint32_t)strtoul(line+1,0,10);pump();response();}
        else if(line[0]=='Q') {
            printf("{\"begins\":%u,\"writes\":%u,\"finishes\":%u,\"aborts\":%u,\"fences\":%u,\"file_bytes\":%lu,\"storage_hex\":\"",begins,writes,finishes,aborts,fences,(unsigned long)saved);
            for(i=0U;i<saved;i++){printf("%02x",storage[i]);}printf("\"}\n");fflush(stdout);
        } else {check(0);}
    }
    return 0;
}

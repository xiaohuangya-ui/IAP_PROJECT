#include "boot_protocol.h"
#include "uart_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,busy,failed,fence_count,txfail,refuse;
static uint32_t errors,tick;
static const char *input;
static char output[256];
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"line %d %s\n",__LINE__,#x);exit(1);}}while(0)
int Uart_Start(void){return 1;}
void Uart_Service(void){}
uint32_t Uart_TakeErrors(void){uint32_t e=errors;errors=0;return e;}
int Uart_TxBusy(void){return busy!=0;}
int Uart_TxComplete(void){return !busy && !failed;}
int Uart_TxSubmit(const uint8_t *p,uint16_t n)
{C(n<sizeof(output));if(busy){return 0;}if(refuse){--refuse;return 0;}if(txfail){txfail=0;failed=1;errors=UART_ERR_TX_START;return 0;}memcpy(output,p,n);output[n]=0;failed=0;return 1;}
int Uart_RxFence(void){fence_count++;input="";return 1;}
int Uart_ReadByte(uint8_t *b){if(!input || !*input){return 0;}*b=(uint8_t)*input++;return 1;}
static void feed(const char *s){input=s;while(*input){BootProtocol_Poll();}BootProtocol_Poll();}
static int begin(void *c,uint32_t n){(void)c;(void)n;return 0;}
static int write_data(void *c,uint32_t o,const uint8_t *p,uint16_t n){(void)c;(void)o;(void)p;(void)n;return 0;}
static int finish(void *c,const ImageIdentity *id){(void)c;(void)id;return 0;}
static void abort_data(void *c){(void)c;}
static int tx(void *c,const uint8_t *p,uint16_t n){(void)c;return Uart_TxSubmit(p,n);}
static uint32_t clock_ms(void *c){(void)c;return tick;}
int main(int argc,char **argv)
{
    YmPort port={0,begin,write_data,finish,abort_data,tx,clock_ms};
    (void)argv;
    if(argc==1) {
    BootProtocol_Init();feed("@IAP:ENTER\r\n");C(!strcmp(output,"ERR NOT_READY\r\n"));
    feed("@IAP:VER\r\n@IAP:LOG\r\n");C(!strcmp(output,"ERR NOT_READY\r\n"));
    txfail=1;feed("@IAP:VER\r\n");C(!failed && !strcmp(output,"ERR UART\r\n"));
    feed("\r\n@IAP:VER\r\n");C(strstr(output,"ACTIVE=UNKNOWN")!=0);
    refuse=1;feed("@IAP:VER\r\n");C(strstr(output,"ACTIVE=UNKNOWN")!=0);
    refuse=100;input="@IAP:VER\r\n@IAP:LOG\r\n";
    {unsigned i;for(i=0;i<40;++i){BootProtocol_Poll();}}
    C(!strcmp(input,"@IAP:LOG\r\n"));refuse=0;BootProtocol_Poll();
    C(strstr(output,"ACTIVE=UNKNOWN")!=0);C(!strcmp(input,"@IAP:LOG\r\n"));
    while(*input){BootProtocol_Poll();}C(!strcmp(output,"ERR NOT_READY\r\n"));
    refuse=100;input="@IAP:VER\r\n";while(*input){BootProtocol_Poll();}
    errors=UART_ERR_OVERFLOW;refuse=0;BootProtocol_Poll();C(!strcmp(output,"ERR UART\r\n"));
    feed("\r\n@IAP:VER\r\n");C(strstr(output,"ACTIVE=UNKNOWN")!=0);
    C(!BootProtocol_SetReceiver(&port));
    } else {
    C(BootProtocol_SetReceiver(&port));C(!BootProtocol_SetReceiver(&port));BootProtocol_Init();
    feed("@IAP:ENTER\r\n@IAP:VER\r\n");C(fence_count==1);BootProtocol_Poll();C(output[0]=='C');
    errors=UART_ERR_OVERFLOW;BootProtocol_Poll();BootProtocol_Poll();C(fence_count==2);BootProtocol_Poll();C(!strcmp(output,"ERR YMODEM\r\n"));
    feed("@IAP:VER\r\n");C(!strcmp(output,"ERR YMODEM\r\n"));feed("@IAP:VER\r\n");C(strstr(output,"ACTIVE=UNKNOWN")!=0);
    }
    printf("{\"result\":\"PASS\",\"assertions\":%u,\"scope\":\"Actual BootProtocol service with UART mock\"}\n",checks);return 0;
}

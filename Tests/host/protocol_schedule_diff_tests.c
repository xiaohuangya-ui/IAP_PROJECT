/* 真实Protocol/Router/YModem连接，UART接受/拒绝/未TC/错误由独立时序驱动。 */
#include "boot_protocol.h"
#include "uart_port.h"
#include <stdio.h>
#include <string.h>
static uint32_t seed=103,tick,errors;
static unsigned busy,failed,refuse,log_rows,reads,queries,closed_count;
static const char *input="";
static char text[64];
static uint32_t next(void){seed=seed*1664525U+1013904223U;return seed;}
int Uart_Start(void){return 1;}
void Uart_Service(void){}
uint32_t Uart_TakeErrors(void){uint32_t e=errors;errors=0;return e;}
int Uart_TxBusy(void){return busy!=0;}
int Uart_TxComplete(void){return !busy&&!failed;}
int Uart_TxSubmit(const uint8_t *p,uint16_t n){
    printf("TX %u %u %u %.*s",tick,busy,refuse,(int)n,p);
    if(busy||refuse){return 0;}busy=2;failed=0;return 1;
}
int Uart_RxFence(void){puts("FENCE");input="";return 1;}
int Uart_ReadByte(uint8_t *b){if(!*input)return 0;*b=(uint8_t)*input++;++reads;return 1;}
static int begin(void *c,uint32_t n){(void)c;(void)n;return 0;}
static int write_data(void *c,uint32_t o,const uint8_t *p,uint16_t n){(void)c;(void)o;(void)p;(void)n;return 0;}
static int finish(void *c,const ImageIdentity *id){(void)c;(void)id;return 0;}
static void abort_data(void *c){(void)c;puts("ABORT");}
static int tx(void *c,const uint8_t *p,uint16_t n){(void)c;return Uart_TxSubmit(p,n);}
static uint32_t clock_ms(void *c){(void)c;return tick;}
static int enter(void){return 1;}
static const char *query(Command cmd){++queries;if(cmd==CMD_IAP_LOG)log_rows=3;sprintf(text,"QUERY %u %u\r\n",(unsigned)cmd,queries);return text;}
static void closed(int ok){++closed_count;printf("CLOSED %d\n",ok);if(!ok)log_rows=0;}
static const char *next_text(void){if(!log_rows)return 0;sprintf(text,"LOG %u\r\n",log_rows--);return text;}
int main(void){unsigned i;static const char *const commands[]={"@IAP:VER\r\n@IAP:STATUS\r\n","@IAP:LOG\r\n@IAP:VER\r\n","@IAP:ENTER\r\n@IAP:VER\r\n","\030\030\r\n@IAP:LOG\r\n","\r\n@IAP:STATUS\r\n"};
    YmPort port={0,begin,write_data,finish,abort_data,tx,clock_ms};IapProtocolControl ctl={enter,query,closed,next_text};
    if(!BootProtocol_SetControl(&ctl)||!BootProtocol_SetReceiver(&port)){return 1;}
    BootProtocol_Init();
    for(i=0;i<1000000;++i){uint32_t r=next();tick+=r%3;
        if(!*input)input=commands[(r>>8)%5];
        refuse=(r>>16)%5==0;
        /* DMA已结束但线路TC尚未来到的期间busy仍为1。 */
        if(busy&&((r>>20)&1))--busy;
        if((r&1023)==1){errors=UART_ERR_FE;failed=1;busy=0;}
        BootProtocol_Poll();
    }
    printf("READS %u QUERIES %u CLOSED %u\n",reads,queries,closed_count);return 0;
}

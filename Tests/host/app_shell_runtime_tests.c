/* 实际AppShell_Run的提交拒绝负例：只mock UART，不替换Router或Shell。 */
#include "app_shell.h"
#include "uart_port.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static jmp_buf done;
static const char input[]="@APP:VERSION\r\n@APP:TEMP\r\n";
static unsigned cursor,steps,attempts,accepted;
static int reject_once,errors_injected;
int Uart_Start(void){return 1;}
void Uart_Service(void){if(++steps>200U){longjmp(done,1);}}
uint32_t Uart_TakeErrors(void){if(reject_once==3 && attempts==1U && !errors_injected){errors_injected=1;return UART_ERR_DMA;}return 0;}
int Uart_TxBusy(void){return 0;}
int Uart_ReadByte(uint8_t *b){if(cursor>=sizeof(input)-1U){return 0;}*b=(uint8_t)input[cursor++];return 1;}
int Uart_TxSubmit(const uint8_t *p,uint16_t n)
{
    attempts++;
    if(reject_once==2 || (reject_once && attempts==1U)){return 0;}
    if(errors_injected){
        if(n!=10U || memcmp(p,"ERR UART\r\n",10U)){fprintf(stderr,"stale reply after fault\n");exit(3);}
        accepted++;return 1;
    }
    if(n!=15U || memcmp(p,"ERR NOT_READY\r\n",15U)){fprintf(stderr,"bad reply\n");exit(2);}
    accepted++;return 1;
}
int main(int argc,char **argv)
{
    reject_once=argc>1?atoi(argv[1]):0;
    if(!setjmp(done)){AppShell_Run();}
    printf("attempts=%u accepted=%u consumed=%u\n",attempts,accepted,cursor);
    if(reject_once==2){return accepted==0U && cursor==14U?0:4;}
    if(reject_once==3){return accepted==1U && errors_injected?0:5;}
    if(accepted!=2U){fprintf(stderr,"FAIL: reply lost after rejected submit\n");return 1;}
    return 0;
}

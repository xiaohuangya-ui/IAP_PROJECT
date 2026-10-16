#include "app_shell.h"
#include <stdio.h>
#include <string.h>
#ifndef PROTOCOL_HOST_TEST
#include "uart_port.h"
static Router router;
static AppShellStatus snapshot; /* Phase9尚无业务/可信确认，不构造假NORMAL。 */
static char response[128];
static uint16_t response_length;
static int dispatch(void *context,Command cmd)
{
    int n;(void)context;n=AppShell_Reply(cmd,&snapshot,response,sizeof(response));
    /* 回复归Shell所有；owner接受前不得消费下一命令或覆盖response。 */
    if(n){response_length=(uint16_t)n;}return 0;
}
void AppShell_Run(void)
{
    uint8_t b,fault=0U;response_length=0U;Router_Init(&router,0,0,dispatch,0);(void)Uart_Start();
    for(;;){Uart_Service();if(Uart_TakeErrors()){Router_AsciiFence(&router);fault=1U;response_length=0U;}
        if(Uart_TxBusy()){continue;}
        if(fault){if(Uart_TxSubmit((const uint8_t *)"ERR UART\r\n",10U)){fault=0U;}continue;}
        if(response_length){
            if(Uart_TxSubmit((const uint8_t *)response,response_length)){response_length=0U;}
            continue;
        }
        if(Uart_ReadByte(&b)){Router_Feed(&router,b);}}
}
#endif
int AppShell_Reply(Command cmd,const AppShellStatus *s,char *out,uint16_t capacity)
{
    const char *fixed=0;int n;
    if (!s || !out || capacity<96U) {return 0;}
    if(cmd==CMD_LINE_LONG){fixed="ERR LINE_TOO_LONG\r\n";}
    else if(cmd==CMD_INVALID){fixed="ERR INVALID\r\n";}
    else if(cmd!=CMD_APP_VERSION && cmd!=CMD_APP_TEMP && cmd!=CMD_APP_UPDATE){fixed="ERR COMMAND\r\n";}
    else if(!s->ready){fixed="ERR NOT_READY\r\n";}
    else if(cmd==CMD_APP_UPDATE){
        if(s->trial || s->busy){fixed="ERR BUSY\r\n";}
        else if(!s->post_update || !s->post_update(s->context)){fixed="ERR NOT_READY\r\n";}
        else {fixed="APP UPDATE REQUESTED\r\n";}
    }
    if(fixed){n=(int)strlen(fixed);memcpy(out,fixed,(unsigned)n+1U);return n;}
    if(cmd==CMD_APP_VERSION){n=snprintf(out,capacity,"APP VER=%lu.%lu MODE=%s\r\n",(unsigned long)(s->version>>16),(unsigned long)(s->version&65535U),s->trial?"TRIAL":"NORMAL");}
    else {n=snprintf(out,capacity,"APP TJ_EST_X10=%ld RAW=%u STATUS=%s\r\n",(long)s->temp_x10,s->raw,s->temp_ok?"OK":"ERR");}
    return n>0 && n<capacity?n:0;
}

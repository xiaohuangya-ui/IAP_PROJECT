#if !defined(PHASE3_TEST_ONLY) && !defined(PHASE4_TEST_ONLY)
#error App shell fixture only
#endif
#include "app_shell.h"
#include "uart_port.h"
static Router router;
static AppShellStatus status; /* 未接业务快照：所有业务请求NOT_READY。 */
static char line[128];
static int command(void *ctx,Command cmd)
{
    int n;(void)ctx;n=AppShell_Reply(cmd,&status,line,sizeof(line));
    if(n){(void)Uart_TxSubmit((const uint8_t *)line,(uint16_t)n);}return 0;
}
void Phase3_AppMain(void)
{
    uint8_t b;Router_Init(&router,0,0,command,0);(void)Uart_Start();
    for(;;){Uart_Service();if(Uart_TakeErrors()){Router_AsciiFence(&router);}
        if(Uart_TxComplete() && Uart_ReadByte(&b)){Router_Feed(&router,b);}}
}

/* 实际Router/AppShell_Reply，仅HOST进程桥，无串口/业务/可信状态伪造。 */
#include "app_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Router router;
static AppShellStatus status;
static char output[1024];static unsigned used;
static int command(void *ctx,Command cmd)
{
    char reply[128];int n;(void)ctx;n=AppShell_Reply(cmd,&status,reply,sizeof(reply));
    if(n<0 || (unsigned)n>sizeof(output)-used){exit(2);}
    memcpy(output+used,reply,(unsigned)n);used+=(unsigned)n;return 0;
}
static unsigned nibble(char c)
{if(c>='0' && c<='9'){return (unsigned)(c-'0');}if(c>='a' && c<='f'){return (unsigned)(c-'a'+10);}exit(3);}
int main(void)
{
    char line[4096];unsigned i;size_t n;
    Router_Init(&router,0,0,command,0);
    while(fgets(line,sizeof(line),stdin)) {
        n=strcspn(line,"\r\n");
        if(line[0]=='W') {
            if((n-1U)%2U){return 4;}
            for(i=1U;i<n;i+=2U){Router_Feed(&router,(uint8_t)(nibble(line[i])*16U+nibble(line[i+1U])));}
        }else if(line[0]!='T'){return 5;}
        for(i=0U;i<used;i++){printf("%02x",(unsigned char)output[i]);}
        used=0U;putchar('\n');fflush(stdout);
    }
    return 0;
}

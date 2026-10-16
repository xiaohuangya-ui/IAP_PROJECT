#include "router.h"
#include "app_shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned checks,calls,binary,posted;
static Command last;
static int enter;
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"line %d %s\n",__LINE__,#x);exit(1);}}while(0)
static int cmd(void *ctx,Command c){C(ctx==0);calls++;last=c;return enter;}
static void bin(void *ctx,uint8_t b){(void)ctx;(void)b;binary++;}
static int post(void *ctx){(void)ctx;posted++;return 1;}
static void feed(Router *r,const char *s){while(*s){Router_Feed(r,(uint8_t)*s++);}}
int main(void)
{
    Router r;unsigned i,k;char out[128];AppShellStatus s={0};
    Router_Init(&r,1,0,cmd,bin);feed(&r,"@IAP:VER\r");C(calls==0);feed(&r,"\n@IAP:STATUS\r\n");C(calls==2 && last==CMD_IAP_STATUS);
    feed(&r,"@APP:UPDATE\r\n");C(last==CMD_UNKNOWN);feed(&r,"@iap:VER\r\n");C(last==CMD_UNKNOWN);
    feed(&r,"@IAP:VER\n@IAP:ENTER\r\n");C(last==CMD_INVALID && r.mode==ROUTE_ASCII);
    feed(&r,"@IAP:VER\r\n");C(last==CMD_IAP_VER);
    for(i=0;i<126;i++){Router_Feed(&r,'A');}C(last==CMD_LINE_LONG);k=calls;feed(&r,"@IAP:ENTER\r\n");C(calls==k);feed(&r,"@IAP:VER\r\n");C(last==CMD_IAP_VER);
    feed(&r,"@IAP:");Router_Feed(&r,0);feed(&r,"VER\r\n");C(last==CMD_INVALID);
    enter=1;feed(&r,"@IAP:ENTER\r\n");C(r.mode==ROUTE_YMODEM);k=calls;
    feed(&r,"@IAP:VER\r\n@APP:UPDATE\r\n");C(calls==k && binary==23);
    Router_AsciiFence(&r);feed(&r,"@IAP:ENTER\r\n");C(calls==k);feed(&r,"@IAP:VER\r\n");C(last==CMD_IAP_VER);
    /* 每个切分位置的半包以及125/126正文界限，真实Parser，不使用IDLE。 */
    for(i=0;i<16;i++){Router_Init(&r,1,0,cmd,bin);k=calls;feed(&r,"@IAP:VER\r\n");C(calls==k+1);}
    Router_Init(&r,0,0,cmd,bin);feed(&r,"@APP:UPDATE\r\n");C(r.mode==ROUTE_ASCII && last==CMD_APP_UPDATE);
    C(AppShell_Reply(CMD_APP_VERSION,&s,out,128)>0 && !strcmp(out,"ERR NOT_READY\r\n"));
    s.ready=1;s.version=0x10002;s.temp_x10=-123;s.raw=4095;s.temp_ok=1;s.post_update=post;
    C(AppShell_Reply(CMD_APP_VERSION,&s,out,128)>0 && !strcmp(out,"APP VER=1.2 MODE=NORMAL\r\n"));
    C(AppShell_Reply(CMD_APP_TEMP,&s,out,128)>0 && !strcmp(out,"APP TJ_EST_X10=-123 RAW=4095 STATUS=OK\r\n"));
    s.trial=1;C(AppShell_Reply(CMD_APP_UPDATE,&s,out,128)>0 && !strcmp(out,"ERR BUSY\r\n") && posted==0);
    s.trial=0;s.busy=1;C(AppShell_Reply(CMD_APP_UPDATE,&s,out,128)>0 && posted==0);
    s.busy=0;C(AppShell_Reply(CMD_APP_UPDATE,&s,out,128)>0 && posted==1);
    C(!AppShell_Reply(CMD_APP_VERSION,&s,out,1));
    /* 非打印字节不能把后缀中的命令变成合法行；每个非法字节后验证恢复。 */
    for(i=0;i<256;i++) {
        if(i>=32 && i<=126){continue;}if(i=='\r' || i=='\n'){continue;}
        Router_Init(&r,1,0,cmd,bin);enter=0;feed(&r,"@IAP:");Router_Feed(&r,(uint8_t)i);
        k=calls;feed(&r,"@IAP:ENTER\r\n");C(calls==k && r.mode==ROUTE_ASCII);
        feed(&r,"@IAP:VER\r\n");C(last==CMD_IAP_VER);
    }
    /* 非法CR序列、最大允许正文125B、超界126B并对完整边界重新同步。 */
    Router_Init(&r,1,0,cmd,bin);feed(&r,"@IAP:VER\rX@IAP:ENTER\r\n");C(last==CMD_INVALID);
    feed(&r,"@IAP:VER\r\n");C(last==CMD_IAP_VER);
    for(i=0;i<125;i++){Router_Feed(&r,'A');}feed(&r,"\r\n");C(last==CMD_UNKNOWN);
    printf("{\"assertions\":%u,\"result\":\"PASS\"}\n",checks);return 0;
}

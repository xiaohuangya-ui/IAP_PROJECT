#include "router.h"
#include <string.h>
/* 0=通用oracle，1=Boot，2=App；角色只影响本owner的命令识别。 */
#ifndef ROUTER_ROLE
#define ROUTER_ROLE 0
#endif
#if ROUTER_ROLE < 0 || ROUTER_ROLE > 2
#error Invalid ROUTER_ROLE
#endif
#ifdef PROTOCOL_PRODUCT_BINDING
#if ROUTER_ROLE != 1
#error Product router requires Boot role
#endif
/* 固定连接只由显式入口建立。通用初始化在此配置拒绝自定义回调。
 * boot是本私有固定连接标志；不依赖无效函数指针或额外逐字节分派。 */
void Router_Init(Router *r,int boot,void *context,CommandHandler command,BinaryHandler binary)
{(void)boot;(void)context;(void)command;(void)binary;memset(r,0,sizeof(*r));}
void Router_InitProduct(Router *r){memset(r,0,sizeof(*r));r->boot=1U;}
#define COMMAND_READY(r) ((r)->boot)
#define BINARY_READY(r) ((r)->boot)
#define COMMAND_CALL(r,c) BootProtocol_ProductCommand(c)
#define BINARY_CALL(r,b) BootProtocol_ProductBinary(b)
#else
void Router_Init(Router *r,int boot,void *context,CommandHandler command,BinaryHandler binary)
{
    memset(r,0,sizeof(*r));r->boot=(uint8_t)(boot!=0);r->context=context;r->command=command;r->binary=binary;
#if ROUTER_ROLE != 0
    /* 错配初始化保持无回调停驻，不能取得另一角色的命令或二进制权限。 */
    if (r->boot != (ROUTER_ROLE==1)) {r->command=0;r->binary=0;}
#endif
}
#define COMMAND_READY(r) ((r)->command)
#define BINARY_READY(r) ((r)->binary)
#define COMMAND_CALL(r,c) (r)->command((r)->context,c)
#define BINARY_CALL(r,b) (r)->binary((r)->context,b)
#endif
void Router_AsciiFence(Router *r)
{
    r->used=0U;r->cr=0U;r->drop=1U;r->mode=ROUTE_ASCII;
}
static Command identify(const Router *r)
{
#if ROUTER_ROLE != 2
#if ROUTER_ROLE == 0
    if (r->boot) {
#endif
        if (!strcmp(r->line,"@IAP:VER")) {return CMD_IAP_VER;}
        if (!strcmp(r->line,"@IAP:STATUS")) {return CMD_IAP_STATUS;}
        if (!strcmp(r->line,"@IAP:ENTER")) {return CMD_IAP_ENTER;}
        if (!strcmp(r->line,"@IAP:LOG")) {return CMD_IAP_LOG;}
#if ROUTER_ROLE == 0
    } else {
#endif
#endif
#if ROUTER_ROLE != 1
        if (!strcmp(r->line,"@APP:VERSION")) {return CMD_APP_VERSION;}
        if (!strcmp(r->line,"@APP:TEMP")) {return CMD_APP_TEMP;}
        if (!strcmp(r->line,"@APP:UPDATE")) {return CMD_APP_UPDATE;}
#ifdef APP_HISTORY_ENABLED
        if (!strcmp(r->line,"@APP:STORE")) {return CMD_APP_STORE;}
        if (!strcmp(r->line,"@APP:HISTORY")) {return CMD_APP_HISTORY;}
#endif
#if ROUTER_ROLE == 0
    }
#endif
#endif
    return CMD_UNKNOWN;
}
void Router_Feed(Router *r,uint8_t byte)
{
    Command cmd;
    if (r->mode==ROUTE_YMODEM) {if(BINARY_READY(r)){BINARY_CALL(r,byte);}return;}
    if (r->drop) {
        if (r->cr && byte=='\n') {r->drop=r->cr=0U;}
        else {r->cr=(uint8_t)(byte=='\r');}
        return;
    }
    if (r->cr && byte=='\n') {
        r->line[r->used]='\0';cmd=identify(r);r->used=r->cr=0U;
        if (COMMAND_READY(r) && COMMAND_CALL(r,cmd) && cmd==CMD_IAP_ENTER && r->boot) {
            r->mode=ROUTE_YMODEM;
        }
        return;
    }
    if (byte=='\r' && !r->cr) {r->cr=1U;return;}
    if (r->cr || byte<32U || byte>126U) {
        r->used=0U;r->drop=1U;r->cr=(uint8_t)(byte=='\r');
        if(COMMAND_READY(r)){(void)COMMAND_CALL(r,CMD_INVALID);}return;
    }
    /* 125B正文+CRLF+NUL=128；第126正文已不可能形成合法行。 */
    if(r->used>=125U) {
        r->used=0U;r->drop=1U;
        if(COMMAND_READY(r)){(void)COMMAND_CALL(r,CMD_LINE_LONG);}return;
    }
    r->line[r->used++]=(char)byte;
}

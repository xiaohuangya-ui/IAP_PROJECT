#include "router.h"
#include <string.h>
void Router_Init(Router *r,int boot,void *context,CommandHandler command,BinaryHandler binary)
{
    memset(r,0,sizeof(*r));r->boot=(uint8_t)(boot!=0);r->context=context;r->command=command;r->binary=binary;
}
void Router_AsciiFence(Router *r)
{
    r->used=0U;r->cr=0U;r->drop=1U;r->mode=ROUTE_ASCII;
}
static Command identify(const Router *r)
{
    if (r->boot) {
        if (!strcmp(r->line,"@IAP:VER")) {return CMD_IAP_VER;}
        if (!strcmp(r->line,"@IAP:STATUS")) {return CMD_IAP_STATUS;}
        if (!strcmp(r->line,"@IAP:ENTER")) {return CMD_IAP_ENTER;}
        if (!strcmp(r->line,"@IAP:LOG")) {return CMD_IAP_LOG;}
    } else {
        if (!strcmp(r->line,"@APP:VERSION")) {return CMD_APP_VERSION;}
        if (!strcmp(r->line,"@APP:TEMP")) {return CMD_APP_TEMP;}
        if (!strcmp(r->line,"@APP:UPDATE")) {return CMD_APP_UPDATE;}
    }
    return CMD_UNKNOWN;
}
void Router_Feed(Router *r,uint8_t byte)
{
    Command cmd;
    if (r->mode==ROUTE_YMODEM) {if(r->binary){r->binary(r->context,byte);}return;}
    if (r->drop) {
        if (r->cr && byte=='\n') {r->drop=r->cr=0U;}
        else {r->cr=(uint8_t)(byte=='\r');}
        return;
    }
    if (r->cr && byte=='\n') {
        r->line[r->used]='\0';cmd=identify(r);r->used=r->cr=0U;
        if (r->command && r->command(r->context,cmd) && cmd==CMD_IAP_ENTER && r->boot) {
            r->mode=ROUTE_YMODEM;
        }
        return;
    }
    if (byte=='\r' && !r->cr) {r->cr=1U;return;}
    if (r->cr || byte<32U || byte>126U) {
        r->used=0U;r->drop=1U;r->cr=(uint8_t)(byte=='\r');
        if(r->command){(void)r->command(r->context,CMD_INVALID);}return;
    }
    /* 125B正文+CRLF+NUL=128；第126正文已不可能形成合法行。 */
    if(r->used>=125U) {
        r->used=0U;r->drop=1U;
        if(r->command){(void)r->command(r->context,CMD_LINE_LONG);}return;
    }
    r->line[r->used++]=(char)byte;
}

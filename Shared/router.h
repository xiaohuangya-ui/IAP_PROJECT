#ifndef ROUTER_H
#define ROUTER_H
#include <stdint.h>
typedef enum {CMD_UNKNOWN,CMD_APP_VERSION,CMD_APP_TEMP,CMD_APP_UPDATE,CMD_IAP_VER,CMD_IAP_STATUS,CMD_IAP_ENTER,CMD_IAP_LOG,CMD_LINE_LONG,CMD_INVALID,CMD_APP_STORE,CMD_APP_HISTORY} Command;
typedef enum {ROUTE_ASCII,ROUTE_YMODEM} RouteMode;
typedef int (*CommandHandler)(void *context,Command command);
typedef void (*BinaryHandler)(void *context,uint8_t byte);
typedef struct {
    RouteMode mode;
    uint16_t used;
    uint8_t drop,cr,boot;
    void *context;
    CommandHandler command;
    BinaryHandler binary;
    char line[128];
} Router;
/* 单一前台消费者。handler仅完整CRLF时调用；返回1仅允许Boot ENTER进入二进制。
 * 回调不得重入Feed；切换后调用者必须Uart_RxFence，废弃已排队旧模式字节。 */
void Router_Init(Router *r,int boot,void *context,CommandHandler command,BinaryHandler binary);
#ifdef PROTOCOL_PRODUCT_BINDING
void Router_InitProduct(Router *r);
int BootProtocol_ProductCommand(Command command);
void BootProtocol_ProductBinary(uint8_t byte);
#endif
void Router_Feed(Router *r,uint8_t byte);
/* DONE/CANCEL/ERROR及UART丢数后先复位，再开放ASCII；先吞掉一个CRLF边界，
 * 防止剩余二进制中@IAP前缀被当命令。调用者同时废弃物理队列。 */
void Router_AsciiFence(Router *r);
#endif

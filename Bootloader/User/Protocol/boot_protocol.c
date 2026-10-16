#include "control_zi.h"
#include "boot_protocol.h"
#include "router.h"
#include "uart_port.h"
#include <string.h>
static Router router;
static BOOT_CONTROL_ZI const char *pending_reply;
static BOOT_CONTROL_ZI const char *deferred_reply;
static YModem receiver;
#ifndef PROTOCOL_PRODUCT_BINDING
static YmPort receiver_port;
#endif
static BOOT_CONTROL_ZI uint8_t receiver_ready,started;
#ifdef IAP_PRODUCT_ENABLED
#ifdef PROTOCOL_PRODUCT_BINDING
#include "product_boot.h"
static BOOT_CONTROL_ZI uint8_t control_ready;
int BootProtocol_SetControl(const IapProtocolControl *p)
{(void)p;return 0;}
int BootProtocol_BindProduct(void)
{if(started||control_ready){return 0;}control_ready=receiver_ready=1U;return 1;}
#else
static IapProtocolControl control;
int BootProtocol_SetControl(const IapProtocolControl *p)
{if(started||!p||!p->enter||!p->query||!p->closed){return 0;}control=*p;return 1;}
#endif
#endif
#ifdef PROTOCOL_PRODUCT_BINDING
int BootProtocol_SetReceiver(const YmPort *port){(void)port;return 0;}
int BootProtocol_BindReceiverProduct(void)
{if(receiver_ready||started){return 0;}receiver_ready=1U;return 1;}
#else
int BootProtocol_SetReceiver(const YmPort *port)
{
    if(!port || receiver_ready || started || !port->begin || !port->write ||
       !port->finish || !port->abort || !port->tx || !port->clock){return 0;}
    receiver_port=*port;receiver_ready=1U;return 1;
}
#endif
#ifdef PROTOCOL_PRODUCT_BINDING
void BootProtocol_ProductBinary(uint8_t byte){YModem_Feed(&receiver,byte);}
#else
static void binary(void *context,uint8_t byte)
{(void)context;YModem_Feed(&receiver,byte);}
#endif
static void reply(const char *s)
{
    /* 单前台发送者。不得覆盖未到TC的缓冲；本轮服务逐条处理命令。 */
    /* 回复为静态文本或query的静态缓冲；接受前不消费下一命令、不再调用query。 */
    if (!Uart_TxSubmit((const uint8_t *)s,(uint16_t)strlen(s))) {deferred_reply=s;}
}
#ifdef PROTOCOL_PRODUCT_BINDING
int BootProtocol_ProductCommand(Command cmd)
{
#else
static int command(void *context,Command cmd)
{
    (void)context;
#endif
#ifdef IAP_PRODUCT_ENABLED
    if(cmd==CMD_IAP_VER||cmd==CMD_IAP_STATUS||cmd==CMD_IAP_LOG){
#ifdef PROTOCOL_PRODUCT_BINDING
        reply(Product_ControlQuery(cmd));return 0;
#else
        reply(control.query(cmd));return 0;
#endif
    }
#endif
    switch(cmd) {
#ifndef IAP_PRODUCT_ENABLED
    case CMD_IAP_VER: reply("BOOT UART-IAP VER=1 ACTIVE=UNKNOWN\r\n");break;
    case CMD_IAP_STATUS:reply("IAP STATE=UNREADY VALID=UNKNOWN ATTEMPTS=UNKNOWN ERROR=NOT_READY\r\n");break;
#endif
    case CMD_LINE_LONG:reply("ERR LINE_TOO_LONG\r\n");break;
    case CMD_INVALID:reply("ERR INVALID\r\n");break;
    case CMD_IAP_ENTER:
#ifdef IAP_PRODUCT_ENABLED
#ifdef PROTOCOL_PRODUCT_BINDING
        if(!Product_ControlEnter()){reply("ERR NOT_READY\r\n");break;}
#else
        if(!control.enter()){reply("ERR NOT_READY\r\n");break;}
#endif
#endif
        if(receiver_ready &&
#ifdef PROTOCOL_PRODUCT_BINDING
           YModem_StartProduct(&receiver)
#else
           YModem_Start(&receiver,&receiver_port)
#endif
           ){return 1;}
        reply("ERR NOT_READY\r\n");break;
    case CMD_IAP_LOG:reply("ERR NOT_READY\r\n");break;
    default:reply("ERR COMMAND\r\n");break;
    }
    return 0;
}
void BootProtocol_Init(void)
{
#ifdef IAP_PRODUCT_ENABLED
    /* 产品必须先注册已验证的IAP接口，失败不启动UART、不开放命令消费者。
     * 独立维护Target保留原UNKNOWN回退；产品命令仅由唯一control回答。 */
#ifdef PROTOCOL_PRODUCT_BINDING
    if(!control_ready){return;}
#else
    if(!control.query){return;}
#endif
#endif
    if(started){return;}started=1U;
#ifdef PROTOCOL_PRODUCT_BINDING
    Router_InitProduct(&router);
#else
    Router_Init(&router,1,0,command,binary);
#endif
    (void)Uart_Start();
}
void BootProtocol_Poll(void)
{
    uint8_t b;
#ifdef PROTOCOL_PRODUCT_BINDING
    if(!started){return;}
#endif
        Uart_Service();
        if (Uart_TakeErrors()) {
            deferred_reply=0; /* 真实链路故障作废旧回复，重新建立ASCII边界。 */
#ifdef IAP_PRODUCT_ENABLED
#ifdef PROTOCOL_PRODUCT_BINDING
            Product_ControlClosed(0);
#else
            if(control.closed){control.closed(0);}
#endif
#endif
            if(router.mode==ROUTE_YMODEM){receiver.closed=0U;YModem_UartFault(&receiver);}
            else {Router_AsciiFence(&router);pending_reply="ERR UART\r\n";}
        }
        if(router.mode==ROUTE_YMODEM) {
            YModem_Poll(&receiver);
            if((receiver.state==YM_ERROR || receiver.state==YM_CANCELLED || receiver.closed) &&
               !receiver.response_length && Uart_TxComplete()) {
                /* 先停DMA并废弃残留，再复位Router；下一个CRLF只作为同步边界。 */
                (void)Uart_RxFence();Router_AsciiFence(&router);
#ifdef IAP_PRODUCT_ENABLED
#ifdef PROTOCOL_PRODUCT_BINDING
                Product_ControlClosed(receiver.state==YM_DONE);
#else
                if(control.closed){control.closed(receiver.state==YM_DONE);}
#endif
#endif
                pending_reply=receiver.state==YM_DONE?
#ifdef PHASE4_TEST_ONLY
                    "P4 VOLATILE VERIFIED NOT_STORED\r\n":
#else
                    "IAP TRANSPORT DONE NOT_COMMITTED\r\n":
#endif
                    "ERR YMODEM\r\n";
            } else if(YModem_CanRead(&receiver) && Uart_ReadByte(&b)){Router_Feed(&router,b);}
            return;
        }
        if (Uart_TxBusy()) {return;}
        {
            /* 唯一文本调度：拒绝重试优先于结束通知，通知优先于LOG。
             * 只取一条，提交拒绝仍由reply保留；接受前绝不取下一命令。 */
            const char *s=deferred_reply;
            if(s){deferred_reply=0;}
            else if(pending_reply){s=pending_reply;pending_reply=0;}
#ifdef IAP_PHASE8_ENABLED
#ifdef PROTOCOL_PRODUCT_BINDING
            else {s=Product_ControlNextText();}
#else
            else if(control.next_text){s=control.next_text();}
#endif
#endif
            if(s){reply(s);return;}
        }
        /* 每轮最多一个字节；完成命令后TX忙，粘包后续留Ring等待。 */
        if (Uart_ReadByte(&b)) {
            Router_Feed(&router,b);
            if(router.mode==ROUTE_YMODEM && !Uart_RxFence()){YModem_UartFault(&receiver);}
        }
}
void BootProtocol_Run(void)
{
    BootProtocol_Init();for(;;){BootProtocol_Poll();}
}

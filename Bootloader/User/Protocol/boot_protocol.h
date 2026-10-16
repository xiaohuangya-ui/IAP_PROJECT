#ifndef BOOT_PROTOCOL_H
#define BOOT_PROTOCOL_H
#include "ymodem.h"
/* 只允许未来IAP事务owner提供真实sink；Phase4 Release从不注册。
 * port被复制，context必须覆盖整个会话；只能在Run前注册一次。 */
int BootProtocol_SetReceiver(const YmPort *port);
/* 维护入口：仅DMA/命令；没有存储/Policy时拒绝ENTER，不建立伪持久事务。 */
void BootProtocol_Run(void);
/* 单前台非阻塞服务；Init只调用一次。拆分入口便于真实服务负例验证。 */
void BootProtocol_Init(void);
void BootProtocol_Poll(void);
#ifdef IAP_PRODUCT_ENABLED
#include "router.h"
typedef struct {
    int (*enter)(void);
    const char *(*query)(Command command);
    /* success=1：完整YModem结束；0：失败关闭或UART故障，取消所有维护流。
     * 不表示Journal提交/镜像可信；重复失败通知必须幂等。 */
    void (*closed)(int success);
#ifdef IAP_PHASE8_ENABLED
    const char *(*next_text)(void); /* LOG只读流；NULL结束，每次回复接受后才推进。 */
#endif
} IapProtocolControl;
int BootProtocol_SetControl(const IapProtocolControl *control);
#ifdef PROTOCOL_PRODUCT_BINDING
/* 固定产品控制+YModem整条连接一次绑定，无自定义Port；单前台、Init前调用。
 * 此配置SetControl始终拒绝，不能与generic注册竞争owner。 */
int BootProtocol_BindProduct(void);
int BootProtocol_BindReceiverProduct(void);
#endif
#endif
#endif

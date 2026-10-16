#ifndef APP_SHELL_H
#define APP_SHELL_H
#include "router.h"
/* App命令只依赖显式快照/投递接口。尚无Phase9业务、确认状态时返回NOT_READY。
 * post_update由未来UpgradeCtrl拥有，不能在Shell写BKP/复位。 */
typedef struct {
    uint32_t version;
    int ready,trial,busy,temp_ok;
    int32_t temp_x10;
    uint16_t raw;
    int (*post_update)(void *context);
    void *context;
} AppShellStatus;
int AppShell_Reply(Command cmd,const AppShellStatus *status,char *out,uint16_t capacity);
void AppShell_Run(void);
#endif

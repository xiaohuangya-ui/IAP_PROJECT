#ifndef APP_HISTORY_H
#define APP_HISTORY_H
#include <stdint.h>
/* App业务区：用户本轮授权从Reserved划出1MiB。升级区0..0x025FFF永不访问。
 * 仅Business任务调用本模块；UART只读取已发布快照，不访问SPI或介质。
 * 32B扇区头+127条32B记录，256扇区循环；一分钟一条有效结温采样。
 * 序号/代数耗尽失败关闭，不回绕；CRC仅检错，不是来源鉴权。 */
#define APP_HISTORY_BASE 0x026000U
#define APP_HISTORY_BYTES 0x100000U
typedef struct {
    uint32_t sequence,version,uptime_ms;
    int32_t temp_x10;
    uint32_t raw;
} AppHistoryRecord;
typedef struct {
    uint32_t state,sequence,corrupt,scanned;
    uint8_t count;
    AppHistoryRecord recent[4]; /* 按旧到新；只是查询缓存，不是持久权威。 */
} AppHistoryView;
enum {HISTORY_WAIT=0,HISTORY_SCAN=1,HISTORY_READY=2,HISTORY_ERROR=3};
typedef struct {
    int (*start)(void);
    int (*read)(uint32_t offset,uint8_t *out,uint32_t bytes);
    int (*program)(uint32_t offset,const uint8_t *data,uint32_t bytes);
    int (*erase_sector)(uint32_t offset);
} AppHistoryPort;
/* Bind无介质副作用；poll每500ms任务级调用，禁止ISR/临界区/重入。
 * allowed=0时不初始化/不读写：Trial、UPDATE、核心业务未健康时暂停。
 * 本模块的成功/失败不加入原Trial确认条件，不直接喂狗或写BKP。
 * 端口只能接受业务区内相对字节偏移；失败锁停至下次复位。 */
void AppHistory_Bind(const AppHistoryPort *port);
void AppHistory_Poll(uint32_t now,uint32_t version,int32_t temp_x10,uint16_t raw,int allowed);
void AppHistory_Get(AppHistoryView *out);
#endif

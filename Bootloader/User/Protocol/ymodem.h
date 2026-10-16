#ifndef YMODEM_H
#define YMODEM_H
#include "fw_header.h"
#define YM_PACKET_TIMEOUT_MS 1000U
#define YM_HANDSHAKE_TIMEOUT_MS 30000U
#define YM_MAX_CONSECUTIVE_ERRORS 10U
#define YM_FINAL_ACK_GRACE_MS 2000U
typedef enum {YM_OFF,YM_BLOCK0,YM_DATA,YM_EOT2,YM_FINAL,YM_DONE,YM_ERROR,YM_CANCELLED} YmState;
typedef enum {YM_ERR_NONE,YM_ERR_PACKET,YM_ERR_LENGTH,YM_ERR_HEADER,YM_ERR_IMAGE,YM_ERR_SINK,YM_ERR_TIMEOUT,YM_ERR_CANCEL,YM_ERR_UART,YM_ERR_TX} YmError;
typedef struct {
    void *context;
    /* 前台有界接口；begin必须先取得真实事务授权，失败不得ACK Block0。
     * write(offset是文件字节偏移)同步完成本地要求的接收/落盘动作后返回1。
     * data仅调用期间有效；不能保留指针。finish不等于download_valid持久提交。
     * 实际W25 sink须finish全量回读验证，Phase6未接入时Release拒绝开始。 */
    int (*begin)(void *,uint32_t file_size);
    int (*write)(void *,uint32_t offset,const uint8_t *data,uint16_t length);
    int (*finish)(void *,const ImageIdentity *identity);
    void (*abort)(void *);
    /* tx必须原子接受1..2B并拥有生命周期；忙返回0，Poll有界重试。
     * clock毫秒，write后重新取时刻，避免正常落盘占用误触下包超时。 */
    int (*tx)(void *,const uint8_t *,uint16_t);
    uint32_t (*clock)(void *);
} YmPort;
typedef struct {
    /* 热控制字段前置，避免跨越两个大缓冲后以宽位移反复访问。
     * 仅RAM内部布局；无packed/缩窄、无协议或持久格式变化。 */
    uint16_t used,needed,previous_length;
    uint8_t expected,previous_id,have_previous,cancel,retries,begun,closed;
    uint8_t response[2],response_length;
    YmState state;
    YmError error;
    uint32_t file_size,received,image_crc,start,wait_since,tx_since,completed_since;
    ImageIdentity identity;
    YmPort port;
    uint8_t vector[8],packet[1029],previous[1024];
} YModem;
/* Session与Packet分离，固定静态空间，无IO介质依赖；唯一前台调用者。 */
int YModem_Start(YModem *y,const YmPort *port);
#ifdef PROTOCOL_PRODUCT_BINDING
int YModem_StartProduct(YModem *y);
#endif
void YModem_Feed(YModem *y,uint8_t byte);
void YModem_Poll(YModem *y);
void YModem_UartFault(YModem *y);
int YModem_CanRead(const YModem *y);
#endif

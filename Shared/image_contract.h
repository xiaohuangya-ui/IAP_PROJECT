#ifndef IMAGE_CONTRACT_H
#define IMAGE_CONTRACT_H
#include <stdint.h>
#define APP_BASE 0x08004000U
#define APP_LIMIT 0xC000U
#define BKP_UPDATE_REQ 0x55AAU
#define BKP_CONFIRM_REQ 0x5AA5U
#define BKP_TRIAL_MARK 0xA33AU
typedef struct {
    uint32_t version;
    uint32_t size;
    uint32_t crc32;
} ImageIdentity;
/* 简介：CRC32/IEEE 增量运算，state 为未执行末尾 XOR 的原始状态。
 * 参数：data 在 length>0 时必须可读；length 单位为字节；空输入允许 NULL。
 * 返回值：原始 CRC 状态；初值/末尾 XOR 均为 0xFFFFFFFF。纯算法，无 IO。 */
uint32_t CRC32_Update(uint32_t state, const uint8_t *data, uint32_t length);
uint32_t CRC32_IEEE(const uint8_t *data, uint32_t length);
/* 简介：统一可启动向量谓词；size 为已可信有效像长度，非整个槽大小。
 * 返回值：1 合法，0 拒绝；仅判定，不建立镜像版本/CRC 信任。 */
int AppVector_Valid(uint32_t size, uint32_t msp, uint32_t pc);
/* 简介：验证调用者提供的可信身份与完整可读镜像，不访问实际 Flash。
 * 参数：available 是 image 实际可读字节数；identity 生命周期覆盖调用。
 * 返回值：1 版本/长度/CRC/向量均匹配，否则 0；非法长度先于数据读取拒绝。 */
int Image_Validate(const uint8_t *image, uint32_t available,
                   const ImageIdentity *identity);
/* P1/P1-D2 纯决策：两轮均16点 HIGH 且配置前后有效才放行。 */
int Recovery_Hold(uint32_t high_a, uint32_t high_b, uint32_t config_ok);
#endif

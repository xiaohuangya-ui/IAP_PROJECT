#ifndef W25Q64_H
#define W25Q64_H
#include <stdint.h>
/* Phase0仅提供0x9F/0x05/0x35只读接口，不编译WREN/写擦/容量探针。
 * 前台调用；失败输出不可信；返回1成功/0失败，参数不得NULL。 */
int W25_ReadJedec(uint32_t *jedec);
int W25_ReadStatus(uint8_t *sr1, uint8_t *sr2);
#ifdef IAP_STORAGE_ENABLED
/* W25Q64BV唯一设备owner扩展；物理8MB边界不等于业务写权限。
 * Init只读JEDEC/保护位；不清保护，不写SR，不复位设备。
 * 返回1=完成且回读一致；失败锁停写擦，需重新Init并重新核实事务。
 * Service用于维护UART，Progress仅在完整介质操作验证后通知进展。
 * 唯一前台owner：Service/Progress回调不得重入任何W25接口。 */
typedef struct {uint32_t (*clock_ms)(void);void (*service)(void);void (*progress)(void);} W25Port;
int W25_Init(const W25Port *port);
#ifdef W25_PRODUCT_BINDING
/* 固定产品单前台入口，时基/Service/Progress不可替换；同配置旧Init拒绝所有端口。
 * Generic构建保留旧参数校验/复制语义；两者都只读核验JEDEC与保护位。 */
int W25_InitProduct(void);
#endif
int W25_Read(uint32_t address,uint8_t *data,uint32_t length);
int W25_Write(uint32_t address,const uint8_t *data,uint32_t length);
int W25_PageProgram(uint32_t address,const uint8_t *data,uint32_t length);
int W25_Erase(uint32_t address,uint32_t bytes); /* 4096或65536，须对齐 */
#endif
#endif

#ifndef SPI_BUS_H
#define SPI_BUS_H
#include <stdint.h>
/* 简介：SPI1前台唯一owner，CubeMX初始化后显式SPE；Mode3/4.5MHz。
 * 返回1配置/CS/SPE正确，0停用；无介质写擦。 */
int SpiBus_Init(void);
/* 简介：一次连续CS的1..4字节同步事务，50ms事务超时。
 * 参数：tx/rx缓冲在返回前有效；失败释放CS并锁错，不自动重试。
 * 返回1成功，0失败；必须IRQ未屏蔽、HAL Tick运行，禁止ISR调用。 */
int SpiBus_Exchange(const uint8_t *tx, uint8_t *rx, uint16_t count);
#ifdef IAP_STORAGE_ENABLED
/* 固定SPI1 Mode3/8bit/MSB-first连续CS，1..260B；命令/地址/数据一起。
 * HAL拥有初始化，本BSP维护Lock/State/ErrorCode/计数并轮询寄存器；
 * 全事务50ms并有3600000次阻塞轮询上限，停Tick仍有限，禁止ISR/屏蔽IRQ。
 * 每发必收、BSY清除后释放CS；失败保留SR现场并锁错，需受控重新初始化。 */
int SpiBus_Transfer(const uint8_t *tx,uint8_t *rx,uint16_t count);
#endif
int SpiBus_Idle(void);
#endif

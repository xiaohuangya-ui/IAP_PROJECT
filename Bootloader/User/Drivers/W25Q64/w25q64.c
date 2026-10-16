#include "w25q64.h"
#include "spi_bus.h"
/* 来源P1-D2 w25q64.c的read_small命令形状；物理总线交由spi_bus/HAL，
 * 排除旧寄存器传输、WriteEnable、擦写、测试mock及Watch门。 */
int W25_ReadJedec(uint32_t *jedec)
{
    const uint8_t tx[4] = {0x9FU, 0xFFU, 0xFFU, 0xFFU};
    uint8_t rx[4];
    if (jedec == 0 || !SpiBus_Exchange(tx, rx, 4U)) { return 0; }
    *jedec = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
    return 1;
}
int W25_ReadStatus(uint8_t *sr1, uint8_t *sr2)
{
    uint8_t tx[2] = {0x05U, 0xFFU}, rx[2];
    if (sr1 == 0 || sr2 == 0 || !SpiBus_Exchange(tx, rx, 2U)) { return 0; }
    *sr1 = rx[1];
    tx[0] = 0x35U;
    if (!SpiBus_Exchange(tx, rx, 2U)) { return 0; }
    *sr2 = rx[1];
    return 1;
}

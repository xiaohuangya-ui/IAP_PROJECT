#include "phase0_entry.h"
#include "phase0_diag.h"
#include "phase0_identity.h"
#include "recovery.h"
#include "backup_domain.h"
#include "spi_bus.h"
#include "w25q64.h"
#include "jump.h"
#include "stm32f1xx_hal.h"
static RecoverySample sample;
static BackupSnapshot before;
static int hold;
static const ImageIdentity whitelist = {P0_APP_VERSION, P0_APP_SIZE, P0_APP_CRC32};
void Phase0_BootEarly(void)
{
    /* 只读保留现场；不清RCC复位标志、不重置备份域。 */
    Backup_ReadSnapshot(&before);
    hold = Recovery_Sample(&sample);
}
__declspec(noreturn) void Phase0_BootMain(void)
{
    uint32_t jedec = 0U;
    uint8_t sr1 = 0xFFU, sr2 = 0xFFU, key;
    int w25_ok, image_ok, bkp_ok = 1;
    JumpTarget target;
    BackupSnapshot after;
    if (!P0_Print("P0 BOOT TEST ONLY\r\n") ||
        !P0_Field("HCLK=", HAL_RCC_GetHCLKFreq()) ||
        !P0_Field("CSR=", before.csr) || !P0_Field("BDCR=", before.bdcr) ||
        !P0_Field("OBR=", FLASH->OBR) ||
        !P0_Field("DR1=", before.dr1) || !P0_Field("DR2=", before.dr2) ||
        !P0_Field("DR3=", before.dr3) ||
        !P0_Field("PB12_A=", sample.high_a) || !P0_Field("PB12_B=", sample.high_b) ||
        !P0_Field("PB12_PRE=", sample.config_pre) || !P0_Field("PB12_POST=", sample.config_post) ||
        !P0_Field("PB12_CYCLES=", sample.cycle_end - sample.cycle_start)) { P0_Stop(); }
    /* #1 不启动硬件看门狗测试；OBR USER bit2=1才是软件IWDG。
     * 实物Option Bytes仍需用户只读核验，不能据此声称IWDG从未启动。 */
    if ((FLASH->OBR & FLASH_OBR_IWDG_SW) == 0U || HAL_RCC_GetHCLKFreq() != 72000000U) { hold = 1; }
    w25_ok = SpiBus_Init() && W25_ReadJedec(&jedec) && W25_ReadStatus(&sr1, &sr2);
    w25_ok = w25_ok && jedec == 0xEF4017U && (sr1 & 3U) == 0U;
    if (!P0_Field("JEDEC=", jedec) || !P0_Field("SR1=", sr1) ||
        !P0_Field("SR2=", sr2) || !P0_Field("W25_OK=", (uint32_t)w25_ok)) { P0_Stop(); }
    /* #2 只消费协议请求，不将DR2解释为产品确认；任何非零请求本次留Boot。
     * 此测试中DR3不会放行Trial；清零后仍要求下次正常复位进入App。 */
    if (before.dr1 != 0U || before.dr2 != 0U || before.dr3 != 0U) {
        hold = 1;
        if (before.dr1 != 0U) { bkp_ok = Backup_BootWrite(1U, 0U) && bkp_ok; }
        if (before.dr2 != 0U) { bkp_ok = Backup_BootWrite(2U, 0U) && bkp_ok; }
        if (before.dr3 != 0U) { bkp_ok = Backup_BootWrite(3U, 0U) && bkp_ok; }
    }
    Backup_ReadSnapshot(&after);
    bkp_ok = bkp_ok && before.bdcr == after.bdcr &&
             (before.pwr & PWR_CR_DBP) == (after.pwr & PWR_CR_DBP) &&
             after.dr1 == 0U && after.dr2 == 0U && after.dr3 == 0U;
    if (!P0_Field("BKP_OK=", (uint32_t)bkp_ok) ||
        !P0_Field("AFTER_DR1=", after.dr1) || !P0_Field("AFTER_DR2=", after.dr2) ||
        !P0_Field("AFTER_DR3=", after.dr3)) { P0_Stop(); }
    /* #3 编译期身份只适用于本独立最小App，不扫描/写入Metadata。 */
    image_ok = Image_Validate((const uint8_t *)APP_BASE, APP_LIMIT, &whitelist);
    if (!P0_Field("APP_SIZE=", whitelist.size) || !P0_Field("APP_CRC=", whitelist.crc32) ||
        !P0_Field("VECTOR_OK=", (uint32_t)AppVector_Valid(whitelist.size,
            *(const uint32_t *)APP_BASE, *(const uint32_t *)(APP_BASE + 4U))) ||
        !P0_Field("IMAGE_OK=", (uint32_t)image_ok)) { P0_Stop(); }
    /* CubeMX/W25初始化后再次核验PB12配置和当前电平，不能覆盖早期HOLD。 */
    if (((GPIOB->CRH >> 16) & 15U) != 8U ||
        (GPIOB->ODR & (1U << 12)) == 0U || (GPIOB->IDR & (1U << 12)) == 0U) { hold = 1; }
    if (!hold && w25_ok && bkp_ok && image_ok && Jump_Prepare(&whitelist, &target)) {
        if (!P0_Print("P0 JUMP\r\n") || !P0_FlushTC()) { P0_Stop(); }
        Jump_Commit(&target);
    }
    if (!P0_Print("P0 HOLD: r=reset, k=DR3 test then reset\r\n")) { P0_Stop(); }
    for (;;) {
        if (!P0_ReadByte(&key)) { continue; }
        if (key == 'r') {
            if (!P0_Print("P0 RESET\r\n") || !P0_FlushTC()) { P0_Stop(); }
            NVIC_SystemReset();
        } else if (key == 'k') {
            if (!P0_Print("P0 DR3 TEST RESET\r\n") || !P0_FlushTC() ||
                !Backup_BootWrite(3U, BKP_TRIAL_MARK)) { P0_Stop(); }
            NVIC_SystemReset();
        }
    }
}

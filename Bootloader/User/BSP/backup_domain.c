#include "backup_domain.h"
#include "image_contract.h"
#include "stm32f1xx.h"
/* P1/P1-D2保留写后回读思想；重构旧无条件关DBP行为，恢复调用前权限。 */
static uint32_t open_clocks(void)
{
    uint32_t old = RCC->APB1ENR & (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN);
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    (void)RCC->APB1ENR;
    __DSB();
    return old;
}
static void close_clocks(uint32_t old)
{
    RCC->APB1ENR = (RCC->APB1ENR & ~(RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN)) | old;
}
void Backup_ReadSnapshot(BackupSnapshot *out)
{
    uint32_t clocks = open_clocks();
    out->dr1 = (uint16_t)BKP->DR1;
    out->dr2 = (uint16_t)BKP->DR2;
    out->dr3 = (uint16_t)BKP->DR3;
    out->bdcr = RCC->BDCR;
    out->csr = RCC->CSR;
    out->pwr = PWR->CR;
    close_clocks(clocks);
}
int Backup_BootWrite(uint32_t reg, uint16_t value)
{
    uint32_t clocks, dbp, i;
    volatile uint32_t *dest;
    int ok;
    if (reg < 1U || reg > 3U ||
        (reg < 3U && value != 0U) ||
        (reg == 3U && value != 0U && value != BKP_TRIAL_MARK) ||
        __get_IPSR() != 0U || __get_PRIMASK() != 0U ||
        __get_BASEPRI() != 0U || __get_FAULTMASK() != 0U ||
        (__get_CONTROL() & 3U) != 0U) { return 0; }
    clocks = open_clocks();
    dbp = PWR->CR & PWR_CR_DBP;
    PWR->CR |= PWR_CR_DBP;
    /* 有限寄存器轮询，不以循环次数宣称毫秒。权限未生效绝不写DRx。 */
    for (i = 0U; i < 1024U && (PWR->CR & PWR_CR_DBP) == 0U; ++i) { __NOP(); }
    dest = reg == 1U ? &BKP->DR1 : (reg == 2U ? &BKP->DR2 : &BKP->DR3);
    ok = 0;
    if ((PWR->CR & PWR_CR_DBP) != 0U) {
        *dest = value;
        __DSB();
        ok = ((uint16_t)*dest == value);
    }
    PWR->CR = (PWR->CR & ~PWR_CR_DBP) | dbp;
    __DSB();
    ok = ok && (PWR->CR & PWR_CR_DBP) == dbp;
    close_clocks(clocks);
    return ok;
}

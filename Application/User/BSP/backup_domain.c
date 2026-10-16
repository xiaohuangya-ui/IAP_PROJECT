#include "backup_domain.h"
#include "image_contract.h"
#include "stm32f1xx.h"
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
uint16_t Backup_AppReadTrial(void)
{
    uint32_t clocks = open_clocks();
    uint16_t result = (uint16_t)BKP->DR3;
    close_clocks(clocks);
    return result;
}
static int request(uint32_t reg,int task_context)
{
    uint32_t clocks, dbp, i;
    uint16_t value;
    volatile uint32_t *dest;
    int ok = 0;
    if ((reg != 1U && reg != 2U) || __get_IPSR() != 0U ||
        __get_PRIMASK() != 0U || __get_BASEPRI() != 0U ||
        __get_FAULTMASK() != 0U ||
        (__get_CONTROL() & 3U) != (task_context ? 2U : 0U)) { return 0; }
    clocks = open_clocks();
    dbp = PWR->CR & PWR_CR_DBP;
    PWR->CR |= PWR_CR_DBP;
    for (i = 0U; i < 1024U && (PWR->CR & PWR_CR_DBP) == 0U; ++i) { __NOP(); }
    dest = reg == 1U ? &BKP->DR1 : &BKP->DR2;
    value = reg == 1U ? BKP_UPDATE_REQ : BKP_CONFIRM_REQ;
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
/* 保留旧MSP前台API及其oracle；产品任务用明确的特化入口。 */
int Backup_AppRequest(uint32_t reg){return request(reg,0);}
int Backup_AppTaskRequest(uint32_t reg){return request(reg,1);}

/* 隔离候选Jump平台契约；MSP/Commit尾段与基线保持。 */
#include "jump.h"
#include "spi_bus.h"
#include "w25q64.h"
#include "uart_ll.h"
#include "stm32f1xx.h"
static int context_ready(void)
{
    return __get_IPSR() == 0U && (__get_CONTROL() & 3U) == 0U &&
           __get_PRIMASK() == 0U && __get_BASEPRI() == 0U &&
           __get_FAULTMASK() == 0U && NVIC->IABR[0] == 0U && NVIC->IABR[1] == 0U;
}
static int peripherals_ready(void)
{
    return (USART1->SR & USART_SR_TC) != 0U &&
           Uart_LL_Quiescent() &&
           (DMA1_Channel4->CCR & DMA_CCR_EN) == 0U &&
           (DMA1_Channel5->CCR & DMA_CCR_EN) == 0U &&
           (RCC->CR & RCC_CR_HSIRDY) != 0U && SpiBus_Idle();
}
int Jump_Prepare(const ImageIdentity *trusted, JumpTarget *out)
{
    uint8_t sr1, sr2;
    const uint32_t *vectors = (const uint32_t *)APP_BASE;
    if (out == 0 || !context_ready() || !peripherals_ready() ||
        !Image_Validate((const uint8_t *)APP_BASE, APP_LIMIT, trusted) ||
        !W25_ReadStatus(&sr1, &sr2) || (sr1 & 3U) != 0U ||
        !peripherals_ready()) { return 0; }
    out->identity = *trusted;
    out->msp = vectors[0];
    out->pc = vectors[1];
    return 1;
}
/* AC5内嵌汇编函数无栈帧。r0/r1已带入MSP/PC，LR换成明确trap地址。
 * MSP切换之后没有PUSH/POP/BL/栈访问；异常屏蔽归零，BX直接去Reset。
 * Reset异常返回时只能落入本函数末尾死循环，不能回Boot调用帧。 */
__asm void Jump_MSPBranch(uint32_t msp, uint32_t pc)
{
    MOVS r2, #0
    ADR.W r3, unexpected_return
    ORR r3, r3, #1
    MOV lr, r3
    MSR CONTROL, r2
    MSR BASEPRI, r2
    MSR FAULTMASK, r2
    MSR MSP, r0
    DSB
    ISB
    CPSIE i
    BX r1
unexpected_return
    CPSID i
    B unexpected_return
}
static __declspec(noreturn) void reset_after_failure(void)
{
    NVIC_SystemReset();
    for (;;) { __NOP(); }
}
__declspec(noreturn) void Jump_Commit(const JumpTarget *target)
{
    JumpTarget verified;
    uint32_t i;
    if (target == 0 || !Jump_Prepare(&target->identity, &verified) ||
        verified.msp != target->msp || verified.pc != target->pc) { reset_after_failure(); }
    /* #1 关闭竞争窗口；不可逆入口以后禁止恢复Boot主循环/调用HAL等待Tick。 */
    __disable_irq();
    if (!peripherals_ready() || NVIC->IABR[0] != 0U || NVIC->IABR[1] != 0U) {
        reset_after_failure();
    }
    /* #2 停请求再停DMA，复位仅Boot拥有的UART/SPI/DMA；句柄不会再被Boot使用。
     * 不复位GPIOA，PA4持续输出高；不写BDCR、不触IWDG/内部Flash。 */
    USART1->CR1 = 0U;
    USART1->CR3 = 0U;
    DMA1_Channel4->CCR = 0U;
    DMA1_Channel5->CCR = 0U;
    DMA1->IFCR = DMA_IFCR_CGIF4 | DMA_IFCR_CGIF5;
    GPIOA->BSRR = (1U << 4);
    SPI1->CR1 &= ~SPI_CR1_SPE;
    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST | RCC_APB2RSTR_SPI1RST;
    RCC->APB2RSTR &= ~(RCC_APB2RSTR_USART1RST | RCC_APB2RSTR_SPI1RST);
    DMA1_Channel4->CNDTR = 0U;
    DMA1_Channel4->CPAR = 0U;
    DMA1_Channel4->CMAR = 0U;
    DMA1_Channel5->CNDTR = 0U;
    DMA1_Channel5->CPAR = 0U;
    DMA1_Channel5->CMAR = 0U;
    RCC->AHBENR &= ~RCC_AHBENR_DMA1EN;
    /* #3 在HSI已就绪前提下回到8MHz安全时钟。有界CPU轮询，无毫秒声称。
     * 先切HSI再关PLL/HSE；Flash等待周期保持2，对8MHz安全，App自行重配。 */
    RCC->CFGR &= ~RCC_CFGR_SW;
    for (i = 0U; i < 100000U && (RCC->CFGR & RCC_CFGR_SWS) != 0U; ++i) { __NOP(); }
    if ((RCC->CFGR & RCC_CFGR_SWS) != 0U) { reset_after_failure(); }
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);
    for (i = 0U; i < 100000U && (RCC->CR & (RCC_CR_PLLRDY | RCC_CR_HSERDY)) != 0U; ++i) { __NOP(); }
    if ((RCC->CR & (RCC_CR_PLLRDY | RCC_CR_HSERDY)) != 0U) { reset_after_failure(); }
    RCC->CFGR = 0U;
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CIR = 0x009F0000U;
    RCC->APB2ENR &= ~(RCC_APB2ENR_USART1EN | RCC_APB2ENR_SPI1EN);
    /* #4 清所有已用中断源、系统挂起/故障状态；VTOR先于MSP，屏障后转移。 */
    EXTI->IMR = 0U;
    EXTI->EMR = 0U;
    EXTI->PR = 0x000FFFFFU;
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    for (i = 0U; i < 2U; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    SCB->SHCSR = 0U;
    SCB->CFSR = SCB->CFSR;
    SCB->HFSR = SCB->HFSR;
    SCB->DFSR = SCB->DFSR;
    SCB->VTOR = APP_BASE;
    __DSB();
    __ISB();
    Jump_MSPBranch(verified.msp, verified.pc);
    reset_after_failure(); /* 编译器保守防御；正常尾段绝不回来。 */
}

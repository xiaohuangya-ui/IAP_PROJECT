#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
USART_TypeDef model_uart;DMA_Channel_TypeDef model_rx,model_tx;DMA_TypeDef model_dma;
GPIO_TypeDef init_gpio;uint32_t SystemCoreClock;
static uint32_t pclk;static unsigned clocks,configures,errors,enabled,priority,checks;
#define C(x) do {checks++;if(!(x)){fprintf(stderr,"line %u\n",(unsigned)__LINE__);exit(1);}}while(0)
void MX_USART1_UART_Init(void);
uint32_t HAL_RCC_GetPCLK2Freq(void) {return pclk;}
void InitModel_EnableClock(unsigned bit) {clocks|=bit;}
void Error_Handler(void) {errors++;}
void HAL_GPIO_Init(GPIO_TypeDef *gpio,const GPIO_InitTypeDef *config)
{
    C(gpio==GPIOA&&clocks==3);configures++;
    if(config->Pin==GPIO_PIN_9) {C(config->Mode==GPIO_MODE_AF_PP&&config->Speed==GPIO_SPEED_FREQ_HIGH);init_gpio.CRH=(init_gpio.CRH&~0xF0U)|0xB0U;}
    else {C(config->Pin==GPIO_PIN_10&&config->Mode==GPIO_MODE_INPUT&&config->Pull==GPIO_PULLUP);init_gpio.ODR|=GPIO_PIN_10;init_gpio.CRH=(init_gpio.CRH&~0xF00U)|0x800U;}
}
void InitModel_SetOutputPin(GPIO_TypeDef *gpio,uint32_t mask)
{
    C(gpio==GPIOA&&clocks==3);C(gpio->CRH==0x13579BDFU);
    gpio->BSRR=(mask>>8)&0xFFFFU;gpio->ODR|=gpio->BSRR;configures++;
}
void HAL_NVIC_SetPriority(unsigned irq,unsigned pre,unsigned sub) {C(irq==37&&pre==6&&!sub);priority=pre;}
void HAL_NVIC_EnableIRQ(unsigned irq) {C(irq==37&&priority==6&&model_uart.BRR==625);enabled++;}
static void setup(void)
{
    memset(&model_uart,0xA5,sizeof(model_uart));memset(&model_rx,0,sizeof(model_rx));memset(&model_tx,0,sizeof(model_tx));
    init_gpio.CRH=0x13579BDF;init_gpio.ODR=0;SystemCoreClock=pclk=72000000U;
    clocks=configures=errors=enabled=priority=0;
}
int main(void)
{
    unsigned i;uint32_t before;
    for(i=0;i<5;i++) {
        setup();before=model_uart.CR1;
        if(i==1) {SystemCoreClock=8000000U;}if(i==2) {pclk=36000000U;}
        if(i==3) {model_rx.CCR=DMA_CCR_EN;}if(i==4) {model_tx.CCR=DMA_CCR_EN;}
        MX_USART1_UART_Init();
        if(i) {C(errors==1&&!clocks&&!configures&&!enabled);C(model_uart.CR1==before);}
        else {C(!errors&&clocks==3&&configures==1&&enabled==1);C(model_uart.BRR==625&&model_uart.CR2==0&&model_uart.CR3==0);
            C(model_uart.CR1==(USART_CR1_UE|USART_CR1_TE|USART_CR1_RE));C((init_gpio.CRH&0xFF0)==0x8B0);C(init_gpio.ODR&GPIO_PIN_10);
            C((init_gpio.CRH&~0xFF0U)==(0x13579BDFU&~0xFF0U));}
    }
    /* 独立算术oracle：72MHz/115200=625，过采样16编码Mantissa39/Fraction1。 */
    C(72000000U/115200U==625U);C((39U<<4|1U)==625U);
    printf("{\"result\":\"PASS\",\"scenarios\":5,\"assertions\":%u}\n",checks);return 0;
}

#include "internal_flash_port.h"
#include "stm32f1xx.h"
#include <string.h>
int FlashPort_Context(void)
{
    return __get_IPSR()==0U && __get_PRIMASK()==0U && __get_BASEPRI()==0U &&
        __get_FAULTMASK()==0U && (__get_CONTROL()&3U)==0U && SystemCoreClock==72000000U;
}
int FlashPort_Read(uint32_t a,uint8_t *p,uint32_t n)
{memcpy(p,(const void *)a,n);return 1;}
const uint8_t *FlashPort_View(uint32_t a)
{return (const uint8_t *)a;}
/* Flash忙时不能从Flash取指。scatter将此唯一操作内核复制到RAM；
 * 内核无库调用/Flash常量，DWT计时不依赖被屏蔽的SysTick。
 * 100ms为软件失败界限；超时后LOCK可能被BSY阻止，返回IF_LOCK，禁止继续。
 * IRQ屏蔽期间UART可能溢出，调用者必须在协议停等且已停RX时擦页。 */
__attribute__((section("FLASH_RAM_CODE"), noinline))
FlashResult FlashPort_Operation(uint32_t a,uint16_t value,int erase)
{
    uint32_t start,old,loops=0,limit=7200000U;FlashResult result=IF_OK;
    if(a<APP_BASE || a>=APP_BASE+APP_LIMIT || (a&1U) || (erase && (a&1023U))){return IF_RANGE;}
    old=__get_PRIMASK();__disable_irq();
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;start=DWT->CYCCNT;
    while(FLASH->SR&FLASH_SR_BSY){
        if((uint32_t)(DWT->CYCCNT-start)>=limit||++loops>=limit){result=IF_BUSY;break;}
    }
    if(result==IF_OK){
        if(FLASH->CR&FLASH_CR_LOCK){FLASH->KEYR=0x45670123U;FLASH->KEYR=0xCDEF89ABU;}
        if(FLASH->CR&FLASH_CR_LOCK){result=IF_LOCK;}
        else{
            FLASH->SR=FLASH_SR_EOP|FLASH_SR_PGERR|FLASH_SR_WRPRTERR;
            start=DWT->CYCCNT;loops=0;
            if(erase){FLASH->CR=FLASH_CR_PER;FLASH->AR=a;FLASH->CR=FLASH_CR_PER|FLASH_CR_STRT;}
            else{FLASH->CR=FLASH_CR_PG;*(volatile uint16_t *)a=value;}
            while(FLASH->SR&FLASH_SR_BSY){
                if((uint32_t)(DWT->CYCCNT-start)>=limit||++loops>=limit){result=IF_TIMEOUT;break;}
            }
            if(FLASH->SR&(FLASH_SR_PGERR|FLASH_SR_WRPRTERR)){result=IF_PROGRAM;}
        }
    }
    FLASH->CR=FLASH_CR_LOCK;
    if(!(FLASH->CR&FLASH_CR_LOCK)){result=IF_LOCK;}
    __DSB();__set_PRIMASK(old);return result;
}

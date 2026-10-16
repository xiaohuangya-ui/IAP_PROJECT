#include "control_zi.h"
#include "boot_iwdg.h"
#include "watchdog_contract.h"
#include "stm32f1xx_hal.h"
#if !defined(IAP_PHASE8_ENABLED)
#error IWDG startup forbidden before Phase8
#endif
static BOOT_CONTROL_ZI uint8_t running,failed;static BOOT_CONTROL_ZI uint32_t last;static LsiMeasurement measurement;
const LsiMeasurement *Iwdg_Measurement(void){return &measurement;}
int Iwdg_Running(void){return running&&!failed;}
int Iwdg_StartMeasured(void)
{
    uint16_t reload;uint32_t start,loops=0;
    if(running||failed||(FLASH->OBR&FLASH_OBR_IWDG_SW)==0U||
       !Lsi_Measure(&measurement)||!Watchdog_Reload(measurement.hz,&reload)){return 0;}
    start=HAL_GetTick();
    while(IWDG->SR){if((uint32_t)(HAL_GetTick()-start)>100U||++loops>7200000U){failed=1U;return 0;}}
    /* 测频/准入完成后先启动内部时钟，再解锁配置；未启动时等待PVU/RVU
     * 可能永不完成。顺序与ST HAL一致；失败后不喂狗，由硬件复位。 */
    IWDG->KR=0xCCCCU;IWDG->KR=0x5555U;IWDG->PR=4U;IWDG->RLR=reload;
    start=HAL_GetTick();loops=0;
    while(IWDG->SR){if((uint32_t)(HAL_GetTick()-start)>100U||++loops>7200000U){failed=1U;return 0;}}
    if(IWDG->PR!=4U||IWDG->RLR!=reload){failed=1U;return 0;}
    IWDG->KR=0xAAAAU;running=1U;last=HAL_GetTick();return 1;
}
void Iwdg_Progress(void)
{
    uint32_t now=HAL_GetTick();
    if(!running||failed){return;}
    if((uint32_t)(now-last)>2000U){failed=1U;return;}
    IWDG->KR=0xAAAAU;last=now;
}

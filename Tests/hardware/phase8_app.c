#ifndef PHASE8_TEST_ONLY
#error Isolated synthetic watchdog confirmation fixture only
#endif
#include "app_entry.h"
#include "watchdog_handoff.h"
#include "backup_domain.h"
#include "image_contract.h"
#include "uart_port.h"
#include "stm32f1xx_hal.h"
#include <string.h>
/* 无ADC/OLED/业务任务。f=一次UART往返进展，c=工装确认，s=永久停喂。
 * 只允许与Phase8 Boot配对；不自行启动IWDG，不能作为产品健康确认。 */
__declspec(noreturn) void App_FatalStop(void){__disable_irq();for(;;){__WFI();}}
__declspec(noreturn) void App_Main(void)
{
    WatchdogHealth health;uint32_t start=HAL_GetTick(),sequence=0;
    int stopped=0,action=0,inflight=0,trial=Backup_AppReadTrial()==BKP_TRIAL_MARK;
    const char *pending="P8 SYNTHETIC TEST ONLY f/c/s\r\n";uint8_t byte;
    AppWatchdog_Init(&health,start,0);(void)AppWatchdog_Early(&health);
    if(!Uart_Start()){App_FatalStop();}
    for(;;){
        Uart_Service();
        if(Uart_TakeErrors()){stopped=1;pending="P8 UART FAIL NO_FEED\r\n";action=0;inflight=0;}
        if(pending){
            if(Uart_TxSubmit((const uint8_t *)pending,(uint16_t)strlen(pending))){pending=0;inflight=1;}
            continue;
        }
        if(inflight){
            if(!Uart_TxComplete()){continue;}inflight=0;
            if(action==1&&!stopped){++sequence;if(!AppWatchdog_Healthy(&health,HAL_GetTick(),sequence,1,0)){stopped=1;}}
            if(action==2&&!stopped){if(Backup_AppRequest(2)){NVIC_SystemReset();}stopped=1;}
            action=0;
        }
        if(!Uart_ReadByte(&byte)){continue;}
        if(byte=='s'){stopped=1;pending="P8 STOPPED NO_FEED\r\n";}
        else if(byte=='f'&&!stopped){action=1;pending="P8 TEST PROGRESS\r\n";}
        else if(byte=='c'&&!stopped&&trial&&sequence>=5U&&(uint32_t)(HAL_GetTick()-start)>=3000U){action=2;pending="P8 SYNTHETIC CONFIRM NOT BUSINESS HEALTH\r\n";}
        else{pending="P8 REFUSED\r\n";}
    }
}
void vApplicationMallocFailedHook(void){App_FatalStop();}
void vApplicationStackOverflowHook(TaskHandle_t task,char *name){(void)task;(void)name;App_FatalStop();}

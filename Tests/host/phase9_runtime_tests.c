/* Windows fibers保留三个真实任务的局部栈/循环；只mock硬件/调度边界。
 * 以逻辑ms驱动任务，不声称FreeRTOS内核或芯片真实时序通过。 */
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_runtime.h"
#include "app_entry.h"
#include "uart_port.h"
#include "adc.h"
#include "i2c.h"
#include "ssd1306.h"
#ifdef APP_HISTORY_ENABLED
#include "app_history.h"
static uint8_t history_medium[APP_HISTORY_BYTES];
static unsigned history_starts,history_writes,history_erases;
#endif
typedef struct {void *fiber;TaskFunction_t fn;uint32_t wake,notified,waiting;} Task;
static Task tasks[3];static void *scheduler;static unsigned current,now,mode,critical;
static unsigned event,request_reg,feed_ms,feeds,led,adc_count,oled_data,tx_ms,tx_active,tx_rejected;
static unsigned input_pos;static char tx_log[8192];static unsigned tx_used;
MockIwdg mock_iwdg;ADC_HandleTypeDef hadc1;I2C_HandleTypeDef hi2c1;
__declspec(noreturn) static void finish(unsigned value){event=value;SwitchToFiber(scheduler);abort();}
void test_critical(int delta){if(delta>0){++critical;}else{assert(critical);--critical;}}
uint32_t __get_IPSR(void){return 0;}
uint32_t __get_PRIMASK(void){return 0;}
uint32_t __get_BASEPRI(void){return critical?0x50:0;}
uint32_t HAL_GetTick(void){return now;}
void HAL_Delay(uint32_t ms){now+=ms;}
void HAL_GPIO_TogglePin(int port,int pin){assert(current==2 && port==GPIOC && pin==GPIO_PIN_13);++led;}
size_t xPortGetFreeHeapSize(void){return 2900;}
size_t xPortGetMinimumEverFreeHeapSize(void){return 2800;}
TickType_t xTaskGetTickCount(void){return now;}
uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t h){(void)h;return 100;}
void vTaskDelay(TickType_t ticks){assert(!critical);tasks[current].wake=now+ticks;SwitchToFiber(scheduler);}
void vTaskDelayUntil(TickType_t *last,TickType_t ticks){*last+=ticks;tasks[current].wake=*last;SwitchToFiber(scheduler);}
void xTaskNotifyGive(TaskHandle_t handle)
{
    Task *t=(Task *)handle;assert(t>=tasks && t<tasks+3);++t->notified;
    if(t->waiting){t->wake=now;}
}
uint32_t ulTaskNotifyTake(int clear,TickType_t ticks)
{
    Task *t=&tasks[current];uint32_t n;assert(!critical && clear);
    if(!t->notified){t->waiting=1;t->wake=now+ticks;SwitchToFiber(scheduler);t->waiting=0;}
    n=t->notified;t->notified=0;return n;
}
#ifdef APP_HISTORY_ENABLED
static int history_start(void){assert(current==2 && !critical);++history_starts;return mode!=21;}
static int history_read(uint32_t o,uint8_t *p,uint32_t n)
{assert(current==2 && !critical && n && o<sizeof(history_medium) && n<=sizeof(history_medium)-o);vTaskDelay(1);memcpy(p,history_medium+o,n);return 1;}
static int history_program(uint32_t o,const uint8_t *p,uint32_t n)
{uint32_t i;assert(current==2 && !critical && n && o<sizeof(history_medium) && n<=sizeof(history_medium)-o);vTaskDelay(3);for(i=0;i<n;i++){assert(history_medium[o+i]==255);history_medium[o+i]&=p[i];++history_writes;}return 1;}
static int history_erase(uint32_t o)
{assert(current==2 && !critical && !(o&4095U) && o<=sizeof(history_medium)-4096U);vTaskDelay(400);memset(history_medium+o,255,4096);++history_erases;return 1;}
const AppHistoryPort *AppHistory_DevicePort(void)
{static const AppHistoryPort p={history_start,history_read,history_program,history_erase};return &p;}
#endif
static VOID WINAPI task_entry(void *arg){Task *t=arg;t->fn(NULL);abort();}
void App_FatalStop(void){finish(9);}
void App_Main(void){abort();}
void NVIC_SystemReset(void){assert(request_reg && !tx_active);finish(1);}
uint16_t Backup_AppReadTrial(void){return mode==1 || mode==7 || mode==8 || (mode>=20 && mode<=22)?0:0xA33A;}
int Backup_AppTaskRequest(uint32_t reg)
{
    assert(current==1 && !critical && !tx_active && now>=tx_ms+2U);
    if(reg==2){assert(now>=3000U && led>=5U && adc_count>=16U && oled_data>=32U);}
    assert(!request_reg);request_reg=reg;return mode==11?0:1;
}
void MX_ADC1_Init(void){}
int HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a){assert(a==&hadc1);return mode==3?1:0;}
int HAL_ADC_Start(ADC_HandleTypeDef *a){assert(a==&hadc1 && current==2 && !critical);return mode==12?1:0;}
int HAL_ADC_Stop(ADC_HandleTypeDef *a){assert(a==&hadc1);return mode==13?1:0;}
int HAL_ADC_PollForConversion(ADC_HandleTypeDef *a,uint32_t timeout){assert(a==&hadc1 && timeout==5U);return mode==2?1:0;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a){assert(a==&hadc1);++adc_count;return mode==14?5000U:1775U+(mode==1?(now/1000U)%20U*4U:0U);}
void MX_I2C1_Init(void){}
void mock_i2c_reset(int a){(void)a;}
int mock_busy(void){return mode==10;}
int HAL_I2C_Init(I2C_HandleTypeDef *h){assert(h==&hi2c1);return 0;}
int HAL_I2C_DeInit(I2C_HandleTypeDef *h){assert(h==&hi2c1);return 0;}
int HAL_I2C_Mem_Write(I2C_HandleTypeDef *h,uint16_t addr,uint16_t control,uint16_t fmt,uint8_t *data,uint16_t size,uint32_t timeout)
{
    assert(current==2 && !critical && h==&hi2c1 && addr==0x78U && fmt==1 && data && size<=32U && timeout==20U);
    if(mode==4){return 1;}if(control==0x40U){assert(size==32U);++oled_data;}return 0;
}
int Uart_Start(void){return 1;}
void Uart_Service(void){assert(current==0);if(tx_active && now>=tx_ms+2U && mode!=6){tx_active=0;}}
void Uart_GetStats(UartStats *s){memset(s,0,sizeof(*s));s->rx_ready=mode!=5;}
uint32_t Uart_TakeErrors(void){return mode==9 && now>=2000U?UART_ERR_DMA:0;}
int Uart_TxBusy(void){return tx_active;}
int Uart_TxComplete(void){return !tx_active;}
int Uart_TxSubmit(const uint8_t *data,uint16_t size)
{
    assert(current==0 && !tx_active && data && size);
    if(mode==15 && !tx_rejected++){return 0;}
    assert(tx_used+size<sizeof(tx_log));memcpy(tx_log+tx_used,data,size);tx_used+=size;tx_log[tx_used]=0;
    tx_ms=now;tx_active=1;return 1;
}
int Uart_ReadByte(uint8_t *b)
{
    const char *input=mode==24?"@APP:HISTORY\r\n@APP:HISTORY\r\n":mode>=20 && mode<=22?"@APP:STORE\r\n@APP:HISTORY\r\n@APP:UPDATE\r\n":mode==7?"@APP:VERSION\r\n@APP:TEMP\r\n@APP:UPDATE\r\n":
      mode==8 || mode==1?"@IAP:ENTER\r\n@APP:VERSION\r\n":"@APP:VERSION\r\n@APP:TEMP\r\n@APP:UPDATE\r\n";
    if(now<(mode>=20 && mode<=22?80000U:mode==24?2999U:1500U) || input_pos>=strlen(input)){return 0;}*b=(uint8_t)input[input_pos++];return 1;
}
void App_StartNativeTasks(TaskFunction_t uart,TaskFunction_t ctrl,TaskFunction_t business)
{
    unsigned i;TaskFunction_t functions[]={uart,ctrl,business};scheduler=ConvertThreadToFiber(NULL);assert(scheduler);
    for(i=0;i<3;++i){tasks[i].fn=functions[i];tasks[i].fiber=CreateFiber(65536,task_entry,&tasks[i]);assert(tasks[i].fiber);}
    app_uart_task=&tasks[0];app_upgrade_task=&tasks[1];app_business_task=&tasks[2];
    feed_ms=now;
    while(!event && now<(mode>=20 && mode<=22?82000U:12000U)){
        for(i=0;i<3 && !event;++i){
            if(mode==16 && i==2 && now>=1500U){continue;}
            if(mode==17 && i==0 && now>=2000U){continue;}
            if(mode==19 && i==1 && now>=1000U && now<3100U){continue;}
            if(now>=tasks[i].wake){current=i;SwitchToFiber(tasks[i].fiber);}
            if(mock_iwdg.KR==0xAAAAU){feed_ms=now;mock_iwdg.KR=0;++feeds;}
        }
        if(!event && now-feed_ms>=4000U){event=2;}
        ++now;
    }
#ifdef APP_HISTORY_ENABLED
    if(mode>=20 && mode<=22){
        assert(event==1 && request_reg==1 && strstr(tx_log,"HISTORY END") && strstr(tx_log,"STORE STATE="));
        if(mode==20){assert(history_writes>=64U && strstr(tx_log,"HISTORY SEQ=1"));}
        else{assert(!history_writes && strstr(tx_log,"STORE STATE=3"));}
        assert(!history_erases);
    }else if(mode==23 || mode==24){assert(event==1 && request_reg==2 && !history_starts && !history_writes);}
    else
#endif
    if(mode==0 || mode==15){assert(event==1 && request_reg==2 && now>=3000U);assert(strstr(tx_log,"ERR BUSY"));}
    else if(mode==7){assert(event==1 && request_reg==1);assert(strstr(tx_log,"APP UPDATE REQUESTED"));}
    else if(mode==1 || mode==8){assert(!request_reg && !event);}
    else if(mode==11){assert(event==2 && request_reg==2);}
    else{assert(event==2 && !request_reg);}
    if(mode==1 && getenv("P9_FRAME_PATH")){FILE *f=fopen(getenv("P9_FRAME_PATH"),"wb");assert(f);assert(fwrite(Ssd_Frame(),1,1024,f)==1024);fclose(f);}
    printf("{\"mode\":%u,\"result\":\"PASS\",\"ms\":%u,\"event\":%u,\"request\":%u,\"feeds\":%u,\"adc_samples\":%u,\"oled_chunks\":%u}\n",mode,now,event,request_reg,feeds,adc_count,oled_data);
    exit(0);
}
int main(int argc,char **argv){mode=argc>1?(unsigned)atoi(argv[1]):0;
#ifdef APP_HISTORY_ENABLED
memset(history_medium,255,sizeof(history_medium));if(mode==22){history_medium[0]=1U;}
#endif
AppRuntime_Run();}

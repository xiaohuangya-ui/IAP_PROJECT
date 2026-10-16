/* Phase9任务编排owner；UART/Business各自单owner。Metadata只归Boot。
 * 本轮授权Business通过AppHistory访问独立业务区，UART不访问介质。
 * UpgradeCtrl独占BKP请求和任务级喂狗。共享状态只在短临界区发布/快照。 */
#include "app_runtime.h"
#include "app_entry.h"
#include "app_version.h"
#include "app_model.h"
#ifdef APP_HISTORY_ENABLED
#include "app_history.h"
#include "app_history_port.h"
#endif
#include "app_shell.h"
#include "app_temperature.h"
#include "app_oled_port.h"
#include "ssd1306.h"
#include "watchdog_handoff.h"
#include "backup_domain.h"
#include "uart_port.h"
#include "adc.h"
#include "i2c.h"
#include <stdio.h>
#include <string.h>
TaskHandle_t app_uart_task,app_upgrade_task,app_business_task;
static AppModel model;
static WatchdogHealth watchdog;
static Router router;
static char response[128];
static uint16_t response_length;
#ifdef APP_HISTORY_ENABLED
static AppHistoryView history_view,history_session;
static uint8_t history_stream,history_index;
/* UART只复制Business发布的只读缓存；不在临界区做SPI/格式化。 */
static AppHistoryView history_snapshot(void)
{AppHistoryView s;taskENTER_CRITICAL();s=history_view;taskEXIT_CRITICAL();return s;}
static void history_next(void)
{
    int n;
    if(history_index<history_session.count){
        const AppHistoryRecord *r=&history_session.recent[history_index++];
        n=snprintf(response,sizeof(response),"HISTORY SEQ=%lu VER=%lu UP_MS=%lu TJ_X10=%ld RAW=%lu\r\n",
            (unsigned long)r->sequence,(unsigned long)r->version,(unsigned long)r->uptime_ms,(long)r->temp_x10,(unsigned long)r->raw);
    }else{n=snprintf(response,sizeof(response),"HISTORY END\r\n");history_stream=0U;}
    response_length=(uint16_t)(n>0 && n<(int)sizeof(response)?n:0);
}
#endif
static volatile uint8_t reset_kind,wire_done,update_ui;
#if APP_RELEASE_VERSION == 2
static AppTrend trend;
#endif
/* 可由用户只读观察；单位words，均为内核真实栈高水位，未冒充硬件测量。 */
volatile uint32_t app_stack_free_words[3];
volatile uint32_t app_heap_free_bytes,app_heap_min_bytes;
static AppModel snapshot(void)
{AppModel s;taskENTER_CRITICAL();s=model;taskEXIT_CRITICAL();return s;}
static void fatal(void)
{taskENTER_CRITICAL();model.fatal=1U;taskEXIT_CRITICAL();}
/* UART任务仅发布升级意图；250ms控制检查与500ms业务显示可能先后跨周期。
 * 不在此处写BKP/复位，更不接收YModem或取得W25权限。 */
static int post_update(void *context)
{
    int accepted=0;(void)context;
    taskENTER_CRITICAL();
    if(!model.trial && !model.fatal && !model.update && reset_kind==0U){model.update=1U;accepted=1;}
    taskEXIT_CRITICAL();
    if(accepted){xTaskNotifyGive(app_business_task);xTaskNotifyGive(app_upgrade_task);}
    return accepted;
}
static int dispatch(void *context,Command cmd)
{
    AppModel s=snapshot();AppShellStatus shell;int n;(void)context;
#ifdef APP_HISTORY_ENABLED
    if(cmd==CMD_APP_STORE || cmd==CMD_APP_HISTORY){
        history_session=history_snapshot();
        if(cmd==CMD_APP_STORE){n=snprintf(response,sizeof(response),"STORE STATE=%lu SEQ=%lu CORRUPT=%lu SCANNED=%lu BYTES=1048576\r\n",
            (unsigned long)history_session.state,(unsigned long)history_session.sequence,(unsigned long)history_session.corrupt,(unsigned long)history_session.scanned);}
        else{history_stream=1U;history_index=0U;n=snprintf(response,sizeof(response),"HISTORY BEGIN COUNT=%u STATE=%lu\r\n",history_session.count,(unsigned long)history_session.state);}
        response_length=(uint16_t)(n>0 && n<(int)sizeof(response)?n:0);return 0;
    }
#endif
    memset(&shell,0,sizeof(shell));shell.version=app_firmware_version;
    shell.ready=s.uart_ready && !s.fatal;shell.trial=s.trial;shell.busy=s.update;
    shell.raw=s.raw;shell.temp_x10=s.temp_x10;shell.temp_ok=s.temp_ok;
    shell.post_update=post_update;
    n=AppShell_Reply(cmd,&shell,response,sizeof(response));response_length=(uint16_t)n;return 0;
}
static void Task_UartRecv(void *unused)
{
    uint8_t b,sent=0U;uint32_t budget;UartStats stats;(void)unused;
    Router_Init(&router,0,0,dispatch,0);
    for(;;){
        Uart_Service();
        if(Uart_TakeErrors()){Router_AsciiFence(&router);response_length=0U;fatal();
#ifdef APP_HISTORY_ENABLED
            history_stream=0U;
#endif
        }
        Uart_GetStats(&stats);
        taskENTER_CRITICAL();model.uart_ready=stats.rx_ready;model.uart_ms=HAL_GetTick();taskEXIT_CRITICAL();
        /* 两种控制消息都由同一UART任务发送；DMA完成不等于线路TC。 */
        if(reset_kind){
#ifdef APP_HISTORY_ENABLED
            /* 复位握手开始后停止新的历史分帧，避免TC后又提交DMA。 */
            history_stream=0U;
#endif
            if(!sent && !response_length && !Uart_TxBusy()){
                const char *text=reset_kind==1U?"APP ENTER BOOT\r\n":"APP HEALTH CONFIRM REQUEST\r\n";
                if(Uart_TxSubmit((const uint8_t *)text,(uint16_t)strlen(text))){sent=1U;}
            }
            if(sent && !wire_done && Uart_TxComplete()){wire_done=1U;xTaskNotifyGive(app_upgrade_task);}
        }
        if(response_length && !Uart_TxBusy()){
            if(Uart_TxSubmit((const uint8_t *)response,response_length)){response_length=0U;}
        }
#ifdef APP_HISTORY_ENABLED
        if(history_stream && !response_length && !Uart_TxBusy()){history_next();}
#endif
        if(!reset_kind && !snapshot().update && !response_length && !Uart_TxBusy()
#ifdef APP_HISTORY_ENABLED
           && !history_stream
#endif
        ){
            for(budget=0U;budget<128U && !response_length;++budget){
                if(!Uart_ReadByte(&b)){break;}Router_Feed(&router,b);
            }
        }
        app_stack_free_words[0]=uxTaskGetStackHighWaterMark(NULL);
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
static void display(const AppModel *s,uint32_t now)
{
    char line[32];int32_t t=s->temp_x10;uint32_t magnitude=(uint32_t)(t<0?-t:t);
    Ssd_Clear();
    if(s->update){Ssd_Text(0U,0U,"ENTER BOOT MODE");Ssd_Text(0U,16U,"UART TX FLUSH");Ssd_Text(0U,32U,"RESTARTING");return;}
    Ssd_Text(0U,0U,APP_VERSION_TEXT);Ssd_Text(42U,0U,s->trial?"TRIAL":"NORMAL");
    (void)snprintf(line,sizeof(line),"TJ~%s%lu.%luC %s",t<0?"-":"",(unsigned long)(magnitude/10U),(unsigned long)(magnitude%10U),s->temp_ok?"OK":"ERR");
    Ssd_Text(0U,16U,line);
#if APP_RELEASE_VERSION == 2
    {
        /* 自有16x14星环像素画；右上角x110..125/y0..13，避开状态文字
         * 与y16开始的温度行。只读28B位图，仍共用唯一framebuffer。 */
        static const uint16_t orbit[]={0x2000U,0x7002U,0x2007U,0x01C2U,
          0x3F70U,0x25D0U,0x3C78U,0x0C18U,0x0F1EU,0x05D2U,
          0x077EU,0x01C0U,0x0000U,0x0000U};
        uint32_t row,col;
        for(row=0U;row<14U;++row){
            for(col=0U;col<16U;++col){
                if(orbit[row]&(1U<<col)){Ssd_Pixel((int)(110U+col),(int)row);}
            }
        }
    }
#endif
#if APP_RELEASE_VERSION == 1
    (void)snprintf(line,sizeof(line),"UP %lus RX %s",(unsigned long)((now-s->start_ms)/1000U),s->uart_ready?"OK":"ERR");
    Ssd_Text(0U,32U,line);
    (void)snprintf(line,sizeof(line),"HEALTH %lu/5 %s",(unsigned long)s->business_seq,s->fatal?"FATAL":(s->oled_ready?"OK":"OLED ERR"));
    Ssd_Text(0U,48U,line);
#else
    {uint32_t i;int x,y,px=0,py=0;int16_t low=0,high=0,value;
    (void)snprintf(line,sizeof(line),"UP %lus HEALTH %s",(unsigned long)((now-s->start_ms)/1000U),s->fatal?"ERR":(s->temp_ok && s->oled_ready?"OK":"WAIT"));
    Ssd_Text(0U,26U,line);
    if(trend.count){low=high=AppTrend_At(&trend,0U);}
    for(i=1U;i<trend.count;++i){value=AppTrend_At(&trend,i);if(value<low){low=value;}if(value>high){high=value;}}
    for(i=0U;i<trend.count;++i){
        /* 已有历史均匀展开到图区，启动仅两个样本时也能看见实际连线。
         * 不补造采样；满60项后仍覆盖同一118像素宽度。 */
        x=trend.count>1U?(int)(i*118U/(trend.count-1U)):0;value=AppTrend_At(&trend,i);
        y=high==low?49:62-(int)(((int32_t)value-low)*26/((int32_t)high-low));
        if(i){
            int step,dx=x-px,dy=y-py,steps=dy<0?-dy:dy;
            /* 按较长轴步进，陡线也逐像素相连；端点在0..118/36..62，
             * steps最多118，乘积有界，不增加framebuffer或任务栈数组。 */
            if(dx>steps){steps=dx;}
            for(step=0;step<=steps;++step){Ssd_Pixel(px+dx*step/steps,py+dy*step/steps);}
        }
        Ssd_Pixel(x,y);px=x;py=y;
    }}
#endif
}
static void Task_Business(void *unused)
{
    TickType_t wake=xTaskGetTickCount();uint32_t cycle=0U;uint16_t raw=0U;int32_t temp=0;
    int adc,oled,due;AppModel s;TickType_t elapsed;(void)unused;
    for(;;){
        /* 通知提前唤醒只用于升级提示，不能伪增业务心跳或改变500ms周期。
         * 绝对周期与无符号差值兼容Tick回绕；超时仍保证正常业务继续运行。 */
        elapsed=xTaskGetTickCount()-wake;
        if(elapsed<pdMS_TO_TICKS(500U)){(void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(500U)-elapsed);}
        elapsed=xTaskGetTickCount()-wake;due=elapsed>=pdMS_TO_TICKS(500U);
        if(due){wake+=pdMS_TO_TICKS(500U);++cycle;}
        s=snapshot();if(due){HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);}
        if((due && (cycle&1U)==0U) || s.update){
            adc=AppTemperature_Read(&raw,&temp);
#if APP_RELEASE_VERSION == 2
            AppTrend_Push(&trend,temp,adc);
#endif
            taskENTER_CRITICAL();model.raw=raw;model.temp_ok=(uint8_t)adc;
            if(adc){model.temp_x10=temp;++model.adc_ok;}taskEXIT_CRITICAL();
            if(!s.oled_ready){oled=Ssd_Init();}else{oled=1;}
            s=snapshot();display(&s,HAL_GetTick());if(oled){oled=Ssd_Flush();}
            taskENTER_CRITICAL();model.oled_ready=(uint8_t)oled;if(oled){++model.oled_ok;}taskEXIT_CRITICAL();
            if(s.update){update_ui=1U;xTaskNotifyGive(app_upgrade_task);}
        }
        /* 测试变体只阻止健康业务进展；与两个Release显式隔离。 */
#ifndef APP_HEALTH_FAIL_TEST
        if(due){taskENTER_CRITICAL();++model.business_seq;model.business_ms=HAL_GetTick();taskEXIT_CRITICAL();}
#endif
#ifdef APP_HISTORY_ENABLED
        /* NORMAL业务健康后才使用独立业务区。升级提示完成前本任务已结束
         * 介质调用；UpgradeCtrl仍独占复位/IWDG，外部存储不增加确认条件。 */
        s=snapshot();
        AppHistory_Poll(HAL_GetTick(),app_firmware_version,s.temp_x10,s.raw,
            due && !s.trial && !s.update && !s.fatal && s.uart_ready && s.temp_ok && s.oled_ready);
        {AppHistoryView h;AppHistory_Get(&h);taskENTER_CRITICAL();history_view=h;taskEXIT_CRITICAL();}
#endif
        app_stack_free_words[2]=uxTaskGetStackHighWaterMark(NULL);
    }
}
static void Task_UpgradeCtrl(void *unused)
{
    AppModel s;uint32_t now,request_ms=0U;int healthy;(void)unused;
    taskENTER_CRITICAL();model.start_ms=HAL_GetTick();taskEXIT_CRITICAL();
    for(;;){
        now=HAL_GetTick();s=snapshot();
        /* 喂狗资格过期不可因任务恢复而重新授予，更不能仍发健康确认。 */
        if((uint32_t)(now-watchdog.last_feed_ms)>2000U){fatal();s.fatal=1U;}
        healthy=AppModel_UartFresh(&s,now) && !s.fatal &&
            (s.business_seq==0U || (uint32_t)(now-s.business_ms)<=1000U);
        /* 试运行缺必需外设时停止后续资格，不无限喂狗卡在Trial。 */
        if(s.trial && (uint32_t)(now-s.start_ms)>=2000U && (!s.adc_ok || !s.oled_ok)){healthy=0;}
        if(healthy){(void)AppWatchdog_Healthy(&watchdog,now,s.business_seq,1,0);}
        if(!reset_kind && !s.fatal && healthy){
            if(s.update && update_ui){reset_kind=1U;request_ms=now;}
            else if(AppModel_Confirm(&s,now)){reset_kind=2U;request_ms=now;}
        }
        if(reset_kind){
            /* 重新核对快照；只有真实线路TC成功后才写请求并复位。 */
            /* DR1/DR2写回成功只证明请求已保存；持久CONFIRMED仍由Boot复核CRC
             * 后提交W25。IWDG已启动且不可停，等待期间仍须真实健康资格。 */
            if(wire_done && healthy && (reset_kind==1U || AppModel_Confirm(&s,now))){
                if(Backup_AppTaskRequest(reset_kind)){NVIC_SystemReset();}fatal();
            }
            if((uint32_t)(now-request_ms)>1000U){fatal();}
        }
        app_stack_free_words[1]=uxTaskGetStackHighWaterMark(NULL);
        app_heap_free_bytes=xPortGetFreeHeapSize();app_heap_min_bytes=xPortGetMinimumEverFreeHeapSize();
        /* 事件降低控制延迟，250ms超时仍检查健康/狗资格；通知本身不授权复位。 */
        (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(250U));
    }
}
__declspec(noreturn) void AppRuntime_Run(void)
{
#ifdef APP_HISTORY_ENABLED
    AppHistory_Bind(AppHistory_DevicePort());
#endif
    AppWatchdog_Init(&watchdog,HAL_GetTick(),0U);
    if(!AppWatchdog_Early(&watchdog)){App_FatalStop();}
    model.trial=(uint8_t)(Backup_AppReadTrial()==0xA33AU);
    if(!Uart_Start()){App_FatalStop();}
    MX_ADC1_Init();MX_I2C1_Init();
    (void)AppTemperature_Init();Ssd_Bind(AppOled_Write,0);
    App_StartNativeTasks(Task_UartRecv,Task_UpgradeCtrl,Task_Business);
}

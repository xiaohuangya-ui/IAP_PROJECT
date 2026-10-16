#ifndef PHASE5_TEST_ONLY
#error Isolated destructive App MVP only
#endif
#include "boot_entry.h"
#include "boot_protocol.h"
#include "internal_flash.h"
#include "uart_port.h"
#include "stm32f1xx_hal.h"
static uint32_t length,received;
static ImageIdentity image;
void Boot_EarlyInit(void){}
static int begin(void *ctx,uint32_t n)
{
    (void)ctx;if(n<40U||n>32U+APP_LIMIT){return 0;}
    length=n;received=0U;return 1;
}
static int write_data(void *ctx,uint32_t offset,const uint8_t *p,uint16_t n)
{
    uint32_t dest;int ok;
    (void)ctx;if(offset!=received||n>length-received){return 0;}
    if(offset==0U){
        if(!FirmwareHeader_Decode(p,n,length,&image)||image.size<8U){return 0;}
    }
    /* 对端尚未获ACK。停收→有界擦写→重建RX→返回落盘结果后才允许ACK。 */
    if(!Uart_PauseReceive()){return 0;}
    ok=1;
    if(offset==0U){
        ok=InternalFlash_Erase(APP_BASE,(image.size+1023U)&~1023U)==IF_OK;
        p+=32U;n=(uint16_t)(n-32U);dest=APP_BASE;
    }else{dest=APP_BASE+offset-32U;}
    if(ok){ok=InternalFlash_Program(dest,p,n)==IF_OK;}
    if(!Uart_ResumeReceive()){ok=0;}
    if(ok){received=offset+n+(offset==0U?32U:0U);}
    return ok;
}
static int finish(void *ctx,const ImageIdentity *id)
{(void)ctx;return received==length && InternalFlash_Verify(id);}
static void abort_rx(void *ctx){(void)ctx;received=0U;}
static int tx(void *ctx,const uint8_t *p,uint16_t n)
{(void)ctx;return Uart_TxSubmit(p,n);}
static uint32_t clock_ms(void *ctx){(void)ctx;return HAL_GetTick();}
__declspec(noreturn) void BootMain(void)
{
    const YmPort port={0,begin,write_data,finish,abort_rx,tx,clock_ms};
    /* 不自动擦写；只在用户ENTER并发送合法文件后执行。无Jump、无IWDG。 */
    (void)BootProtocol_SetReceiver(&port);BootProtocol_Run();for(;;){}
}

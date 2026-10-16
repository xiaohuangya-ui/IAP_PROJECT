#ifndef PHASE6_TEST_ONLY
#error Isolated NOR Download and Backup test only
#endif
#include "boot_entry.h"
#include "boot_protocol.h"
#include "image_store.h"
#include "internal_flash.h"
#include "w25q64.h"
#include "spi_bus.h"
#include "uart_port.h"
#include "stm32f1xx_hal.h"
/* 本夹具只准入190108配套测试App；不是产品Provisioning/Boot Policy。 */
static const ImageIdentity active={0x50330001U,11536U,0xC49303E4U};
static uint32_t length,received;
static void progress(void){} /* Phase6不得启动或假装接力IWDG。 */
static uint32_t clock_ms(void *ctx){(void)ctx;return HAL_GetTick();}
static int begin(void *ctx,uint32_t n)
{
    (void)ctx;if(n<40U||n>APP_LIMIT+32U||!InternalFlash_Verify(&active)){return 0;}
    length=n;received=0;return Store_Erase(STORE_DOWNLOAD,0,65536U);
}
static int write_data(void *ctx,uint32_t off,const uint8_t *p,uint16_t n)
{
    (void)ctx;if(off!=received||n>length-received||!Store_Program(STORE_DOWNLOAD,off,p,n)){return 0;}
    received+=n;return 1;
}
static int finish(void *ctx,const ImageIdentity *id)
{
    ImageIdentity actual;BackupIdentity backup;(void)ctx;
    return received==length&&Download_Verify(length,&actual)&&actual.version==id->version&&
        actual.size==id->size&&actual.crc32==id->crc32&&Backup_Create(&active,&backup);
}
static void abort_rx(void *ctx){(void)ctx;received=0;}
static int tx(void *ctx,const uint8_t *p,uint16_t n)
{(void)ctx;return Uart_TxSubmit(p,n);}
void Boot_EarlyInit(void){}
__declspec(noreturn) void BootMain(void)
{
    const W25Port device={HAL_GetTick,Uart_Service,progress};
    const YmPort sink={0,begin,write_data,finish,abort_rx,tx,clock_ms};
    /* 只有读初始化成功才接收；所有擦写均由明确ENTER和文件触发。 */
    if(SpiBus_Init()&&W25_Init(&device)){(void)BootProtocol_SetReceiver(&sink);}
    BootProtocol_Run();for(;;){}
}

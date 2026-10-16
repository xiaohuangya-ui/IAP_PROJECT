#ifndef PHASE4_TEST_ONLY
#error Volatile test receiver must not enter Release
#endif
#include "boot_protocol.h"
#include "uart_port.h"
#include "stm32f1xx_hal.h"
/* 测试sink仅增量校验/计数，不保存文件，不擦写任何介质。ACK表示测试接收。
 * 可以验证标准Sender协议；无法证明NOR持久化/回读/有效提交。 */
static uint32_t length,received,file_crc;
static int begin(void *ctx,uint32_t n)
{(void)ctx;length=n;received=0U;file_crc=0xFFFFFFFFU;return 1;}
static int write_data(void *ctx,uint32_t offset,const uint8_t *data,uint16_t n)
{
    (void)ctx;if(offset!=received || n>length-received){return 0;}
    file_crc=CRC32_Update(file_crc,data,n);received+=n;return 1;
}
static int finish(void *ctx,const ImageIdentity *id)
{(void)ctx;return received==length && length==32U+id->size;}
static void abort_rx(void *ctx){(void)ctx;received=0U;}
static int tx(void *ctx,const uint8_t *data,uint16_t n)
{(void)ctx;return Uart_TxSubmit(data,n);}
static uint32_t clock_ms(void *ctx){(void)ctx;return HAL_GetTick();}
void Phase4_Main(void)
{
    const YmPort port={0,begin,write_data,finish,abort_rx,tx,clock_ms};
    (void)BootProtocol_SetReceiver(&port);BootProtocol_Run();for(;;){}
}

#include "byte_buffer.h"
#include "control_zi.h"
#include "w25q64.h"
#include "spi_bus.h"
#include <string.h>
#ifdef W25_PRODUCT_BINDING
#include "stm32f1xx_hal.h"
#include "uart_port.h"
extern void Product_StorageProgress(void);
static uint32_t clock_ms(void){return HAL_GetTick();}
static void service(void){Uart_Service();}
static void progress(void){Product_StorageProgress();}
#else
static W25Port port;
static uint32_t clock_ms(void){return port.clock_ms();}
static void service(void){port.service();}
static void progress(void){port.progress();}
#endif
static BOOT_CONTROL_ZI uint8_t ready;static uint8_t txbuf[260],rxbuf[260];
static int range(uint32_t a,uint32_t n){return n && a<0x800000U && n<=0x800000U-a;}
static int io(uint16_t n)
{if(!SpiBus_Transfer(txbuf,rxbuf,n)){ready=0U;return 0;}return 1;}
static void address(uint8_t cmd,uint32_t a)
{
#if defined(__CC_ARM) && !defined(__BIG_ENDIAN)
    /* 所有调用者先通过8MB range检查，a最高字节必为0。
     * REV后低字节正好留给命令；只装配普通RAM中的四字节SPI头。 */
    uint32_t word=__rev(a)|cmd;memcpy(txbuf,&word,sizeof(word));
#else
    txbuf[0]=cmd;txbuf[1]=(uint8_t)(a>>16);txbuf[2]=(uint8_t)(a>>8);txbuf[3]=(uint8_t)a;
#endif
}
static int wait_ready(uint32_t ms)
{
    uint32_t start=clock_ms(),polls=0;uint8_t a,b;
    do{
        service();
        if(!W25_ReadStatus(&a,&b)){break;}
        if(!(a&1U)){return 1;}
        /* 第二上限避免时基停止时无界轮询；不按轮询次数刷新看门狗。 */
    }while((uint32_t)(clock_ms()-start)<ms && ++polls<1000000U);
    ready=0U;return 0;
}
static int write_enable(void)
{
    uint8_t a,b;
    if(!ready || !wait_ready(5U) || !W25_ReadStatus(&a,&b)){ready=0U;return 0;}
    /* BV的BP/TB/SEC/SRP保护及QE均保持安全默认，不擅自清状态寄存器。 */
    if((a&0xFCU)||b){ready=0U;return 0;}
    txbuf[0]=6U;
    if(!io(1U)||!W25_ReadStatus(&a,&b)||(a&3U)!=2U){ready=0U;return 0;}
    return 1;
}
int W25_Init(const W25Port *p)
{
#ifdef W25_PRODUCT_BINDING
    (void)p;ready=0U;return 0;
}
int W25_InitProduct(void)
{
#endif
    uint32_t jedec;uint8_t a,b;ready=0U;
#ifndef W25_PRODUCT_BINDING
    if(!p||!p->clock_ms||!p->service||!p->progress){return 0;}
    port=*p;
#endif
    if(!W25_ReadJedec(&jedec)||jedec!=0xEF4017U||!W25_ReadStatus(&a,&b)||a||b){return 0;}
    ready=1U;return 1;
}
int W25_Read(uint32_t a,uint8_t *p,uint32_t n)
{
    uint32_t k;
    if(!ready||!p||!range(a,n)||!wait_ready(5U)){return 0;}
    while(n){
        k=n>256U?256U:n;address(3U,a);memset(txbuf+4,255,k);
        if(!io((uint16_t)(k+4U))){return 0;}
        /* 内部页校验直接借用已接收的数据；不向同一缓冲执行memcpy。
         * 借用仅持续到下一次io，service回调遵守原有单owner非重入契约。 */
        if(p!=rxbuf+4){memcpy(p,rxbuf+4,k);}p+=k;a+=k;n-=k;service();
    }
    return 1;
}
int W25_PageProgram(uint32_t a,const uint8_t *p,uint32_t n)
{
    if(!p||!range(a,n)||n>256U||n>256U-(a&255U)){return 0;}
    if(!W25_Read(a,rxbuf+4,n)){return 0;}
    if(!Bytes_AllFF(rxbuf+4,n)){return 0;}
    if(!write_enable()){return 0;}
    address(2U,a);memcpy(txbuf+4,p,n);
    /* BV tPP最大3ms，5ms有限软件窗口；ACK只能在此函数成功返回后。 */
    if(!io((uint16_t)(n+4U))||!wait_ready(5U)||!W25_Read(a,rxbuf+4,n)||memcmp(rxbuf+4,p,n)){
        ready=0U;return 0;
    }
    progress();return 1;
}
int W25_Write(uint32_t a,const uint8_t *p,uint32_t n)
{
    uint32_t k;
    if(!p||!range(a,n)){return 0;}
    while(n){k=256U-(a&255U);if(k>n){k=n;}
        if(!W25_PageProgram(a,p,k)){return 0;}a+=k;p+=k;n-=k;}
    return 1;
}
int W25_Erase(uint32_t a,uint32_t n)
{
    uint32_t off;
    if((n!=4096U&&n!=65536U)||!range(a,n)||(a&(n-1U))){return 0;}
    if(!write_enable()){return 0;}
    address(n==4096U?0x20U:0xD8U,a);
    /* BV最坏tSE=400ms(高循环次数)，tBE64=1000ms；留有界调度裕量。 */
    if(!io(4U)||!wait_ready(n==4096U?450U:1100U)){return 0;}
    for(off=0;off<n;off+=256U){
        if(!W25_Read(a+off,rxbuf+4,256U)){return 0;}
        if(!Bytes_AllFF(rxbuf+4,256U)){ready=0U;return 0;}
    }
    progress();return 1;
}

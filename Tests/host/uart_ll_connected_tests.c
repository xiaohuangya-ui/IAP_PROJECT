/* 重用独立寄存器模型；下方接实际所有产品owner，不替换Uart/Protocol/FSM/Store。 */
#define main isolated_uart_suite_main
#include "uart_ll_tests.c"
#undef main
#include "stm32f1xx.h"
#include "spi.h"
#include "spi_bus.h"
#include "product_boot.h"
#include "boot_protocol.h"
#include "iap_fsm.h"
#include "internal_flash_port.h"
#include "backup_domain.h"
#include "recovery.h"
#include "jump.h"
#include "crc16.h"
#include "w25q64.h"
#include "upgrade_log.h"
#include <windows.h>
#include <setjmp.h>
SPI_TypeDef host_spi;GPIO_TypeDef host_gpio;RCC_TypeDef host_rcc;
uint32_t SystemCoreClock=72000000U;
PWR_TypeDef host_pwr;BKP_TypeDef host_bkp;NVIC_TypeDef host_nvic;
SPI_HandleTypeDef hspi1;uint32_t host_control;
static uint8_t nor[0x26000],new_file[1061],old_flash[0xC000];
static uint8_t spi_cmd,spi_reply,nor_sr;static uint32_t spi_address,spi_index;
static unsigned mode,sender,polls,flash_ops,jumps,started_iwdg,log_rows,log_error_injected;
static int did_pause;static jmp_buf finish_test;
static ImageIdentity new_id;static char output[1024];static unsigned output_size;
uint32_t HAL_RCC_GetPCLK2Freq(void) {return 72000000U;}
uint32_t SpiBus_TestStatus(void) {return host_spi.SR;}
void HAL_GPIO_WritePin(GPIO_TypeDef *g,uint32_t pin,uint32_t value)
{
    C(g==GPIOA&&pin==GPIO_PIN_4);
    if(!value) {host_gpio.ODR&=~pin;spi_index=spi_address=0;spi_cmd=0;}
    else {
        host_gpio.ODR|=pin;
        if(spi_cmd==6) {nor_sr=2;}
        if(spi_cmd==2) {nor_sr=0;}
        if(spi_cmd==0x20||spi_cmd==0xD8) {
            unsigned n=spi_cmd==0x20?4096U:65536U;
            C(nor_sr==2&&spi_address+n<=sizeof(nor));memset(nor+spi_address,255,n);nor_sr=0;
        }
    }
}
void SpiBus_TestWrite(uint8_t value)
{
    C(!(host_gpio.ODR&GPIO_PIN_4));C(hspi1.Lock==HAL_LOCKED);spi_reply=0;
    if(!spi_index) {spi_cmd=value;}
    else if(spi_cmd==0x9F) {static const uint8_t id[]={0,0xEF,0x40,0x17};C(spi_index<4);spi_reply=id[spi_index];}
    else if(spi_cmd==5) {spi_reply=nor_sr;}
    else if(spi_cmd==0x35) {spi_reply=0;}
    else if(spi_cmd==3||spi_cmd==2||spi_cmd==0x20||spi_cmd==0xD8) {
        if(spi_index<4) {spi_address=(spi_address<<8)|value;}
        else {
            C(spi_address+spi_index-4<sizeof(nor));
            if(spi_cmd==3) {spi_reply=nor[spi_address+spi_index-4];}
            else {C(spi_cmd==2&&nor_sr==2);nor[spi_address+spi_index-4]&=value;}
        }
    }else {C(spi_cmd==6);}
    spi_index++;host_spi.SR=SPI_SR_TXE|SPI_SR_RXNE;
}
uint8_t SpiBus_TestRead(void) {host_spi.SR=SPI_SR_TXE;return spi_reply;}
int FlashPort_Context(void) {return !mock_ipsr&&!mock_primask;}
const uint8_t *FlashPort_View(uint32_t address) {return (const uint8_t*)address;}
int FlashPort_Read(uint32_t address,uint8_t *p,uint32_t n) {C(address>=APP_BASE&&n<=APP_LIMIT-(address-APP_BASE));memcpy(p,(void*)address,n);return 1;}
FlashResult FlashPort_Operation(uint32_t address,uint16_t value,int erase)
{
    C(address>=APP_BASE&&address<APP_BASE+APP_LIMIT);C(Uart_LL_Quiescent());did_pause=1;flash_ops++;
    if(erase) {memset((void*)address,255,1024);}else {*(uint16_t*)address=value;}return IF_OK;
}
int Recovery_Sample(RecoverySample *p) {memset(p,0,sizeof(*p));return mode!=0;}
int Iwdg_Running(void) {return started_iwdg;}
int Iwdg_StartMeasured(void) {started_iwdg=1;return 1;}
void Iwdg_Progress(void) {}
void Jump_Commit(const JumpTarget *target)
{
    C(Uart_LL_Quiescent());C(target->identity.version==new_id.version);C(InternalFlash_Verify(&new_id));
    C(did_pause&&flash_ops>0);C(Iap_Record()->state==PENDING_CONFIRM&&Iap_Record()->attempts==1);
    C(!memcmp((void*)APP_BASE,new_file+32,new_id.size));jumps++;longjmp(finish_test,1);
}
static void put32(uint8_t *p,uint32_t value) {p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);p[2]=(uint8_t)(value>>16);p[3]=(uint8_t)(value>>24);}
static void prepare_guards(const ImageIdentity *active)
{
    unsigned i;JumpTarget target;uint32_t sr;
    C(Jump_Prepare(active,&target));C(target.identity.crc32==active->crc32);
    C(!Jump_Prepare(active,0));
    for(i=0;i<18;i++) {
        memset(&target,0xEE,sizeof(target));sr=model_uart.SR;
        switch(i) {
        case 0:mock_ipsr=1;break;case 1:host_control=1;break;case 2:mock_primask=1;break;
        case 3:mock_basepri=0x50;break;case 4:mock_faultmask=1;break;
        case 5:host_nvic.IABR[0]=1;break;case 6:host_nvic.IABR[1]=1;break;
        case 7:model_uart.SR=0;break;case 8:model_uart.BRR=0;break;
        case 9:model_rx.CCR=DMA_CCR_EN;break;case 10:model_tx.CCR=DMA_CCR_EN;break;
        case 11:model_uart.CR3=USART_CR3_DMAR;break;case 12:model_uart.CR3=USART_CR3_DMAT;break;
        case 13:model_uart.CR1|=USART_CR1_IDLEIE;break;case 14:host_rcc.CR=0;break;
        case 15:host_spi.SR=SPI_SR_BSY;break;case 16:nor_sr=2;break;
        case 17:((uint8_t*)APP_BASE)[100]^=1;break;
        }
        C(!Jump_Prepare(active,&target));C(target.msp==0xEEEEEEEEU&&target.pc==0xEEEEEEEEU);
        if(i==17) {((uint8_t*)APP_BASE)[100]^=1;}
        mock_ipsr=mock_primask=mock_basepri=mock_faultmask=host_control=0;
        host_nvic.IABR[0]=host_nvic.IABR[1]=0;model_uart.SR=sr;model_uart.BRR=625;
        model_rx.CCR=model_tx.CCR=0;model_uart.CR3=0;model_uart.CR1&=~USART_CR1_IDLEIE;
        host_rcc.CR=RCC_CR_HSIRDY;host_spi.SR=SPI_SR_TXE;nor_sr=0;
    }
}
static void bytes(const uint8_t *p,unsigned n)
{
    unsigned i,pos;uint8_t *rx=(uint8_t*)model_rx.CMAR;
    C(model_rx.CCR&DMA_CCR_EN);C(model_uart.CR3&USART_CR3_DMAR);
    for(i=0;i<n;i++) {
        pos=512-model_rx.CNDTR;rx[pos]=p[i];model_rx.CNDTR--;
        if(model_rx.CNDTR==256) {model_dma.ISR|=DMA_ISR_HTIF5;rx_irq();}
        if(!model_rx.CNDTR) {model_dma.ISR|=DMA_ISR_TCIF5;model_rx.CNDTR=512;rx_irq();}
    }idle_irq();
}
static void packet(uint8_t seq,const uint8_t *p,unsigned length,unsigned block)
{
    uint8_t buf[1029];uint16_t crc;C(length<=block);
    buf[0]=block==128?1:2;buf[1]=seq;buf[2]=(uint8_t)~seq;
    memset(buf+3,0x1A,block);if(length) {memcpy(buf+3,p,length);}
    crc=CRC16_Xmodem(buf+3,block);buf[3+block]=(uint8_t)(crc>>8);buf[4+block]=(uint8_t)crc;
    bytes(buf,block+5);
}
static void collect_tx(void)
{
    if((model_tx.CCR&DMA_CCR_EN)&&(model_uart.CR3&USART_CR3_DMAT)) {
        unsigned n=model_tx.CNDTR;uint8_t *p=(uint8_t*)model_tx.CMAR;C(output_size+n<sizeof(output));
        memcpy(output+output_size,p,n);output_size+=n;output[output_size]=0;
        tx_done();
    }
}
void Connected_Protocol_Poll(void);
void BootProtocol_Poll(void)
{
    uint8_t block0[128]={0},eot=4;
    if(++polls>=50000) {fprintf(stderr,"sender=%u state=%lu output_size=%u\n",sender,(unsigned long)Iap_Record()->state,output_size);C(0);}
    if(polls%10U==0U) {now++;mock_cycles+=72000;}
    if(mode==2) {
        if(!sender) {bytes((const uint8_t*)"@IAP:LOG\r\n",10);sender=1;}
        if(log_rows&&!log_error_injected) {model_uart.SR|=USART_SR_FE;idle_irq();log_error_injected=1;}
    }else {
        if(!sender) {bytes((const uint8_t*)"@IAP:ENTER\r\n",12);sender=1;}
        else if(output_size) {
            if(sender==1&&strchr(output,'C')) {memcpy(block0,"app.bin\0" "1061",12);packet(0,block0,128,128);sender=2;output_size=0;}
            else if(sender==2&&strchr(output,6)&&strchr(output,'C')) {packet(1,new_file,1024,1024);sender=3;output_size=0;}
            else if(sender==3&&strchr(output,6)) {packet(2,new_file+1024,37,128);sender=4;output_size=0;}
            else if(sender==4&&strchr(output,6)) {bytes(&eot,1);sender=5;output_size=0;}
            else if(sender==5&&strchr(output,21)) {bytes(&eot,1);sender=6;output_size=0;}
            else if(sender==6&&strchr(output,6)&&strchr(output,'C')) {packet(0,block0,128,128);sender=7;output_size=0;}
        }
    }
    Connected_Protocol_Poll();collect_tx();
    if(mode==2&&strstr(output,"LOG SEQ=")) {
        if(log_error_injected) {C(0); /* 负例：UART故障后旧LOG会话不得继续输出。 */}
        log_rows++;output_size=0;output[0]=0;
    }
    if(mode==2&&log_error_injected&&polls>100) {
        C(log_rows==1);C(strstr(output,"ERR UART\r\n")!=0);C(!flash_ops&&!jumps);
        longjmp(finish_test,1);
    }
    if((mode==1||mode==3)&&Iap_Record()->state==PENDING_CONFIRM&&flash_ops) {
        C(!Uart_LL_Quiescent());C(model_rx.CCR&DMA_CCR_EN);C(InternalFlash_Verify(&new_id));C(Backup_Verify(&Iap_Record()->backup));
        C(!memcmp(nor+0x10000,old_flash,APP_LIMIT));longjmp(finish_test,1);
    }
}
int main(int argc,char **argv)
{
    unsigned i;ImageIdentity active;MetaScan scan;Metadata record;W25Port port={HAL_GetTick,Uart_Service,Iwdg_Progress};
    C(argc==2);mode=(unsigned)atoi(argv[1]);
    C(VirtualAlloc((void*)0x08000000U,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void*)0x08000000U);
    model_uart.SR=USART_SR_TC;model_uart.BRR=625;model_uart.CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE;
    host_rcc.CR=RCC_CR_HSIRDY;host_spi.CR1=SPI_CR1_MSTR|SPI_CR1_CPOL|SPI_CR1_CPHA|SPI_CR1_BR_0|SPI_CR1_BR_1|SPI_CR1_SSM|SPI_CR1_SSI;
    host_spi.SR=SPI_SR_TXE;host_gpio.CRL=2U<<16;host_gpio.ODR=GPIO_PIN_4;hspi1.Instance=SPI1;hspi1.State=HAL_SPI_STATE_READY;
    memset(nor,255,sizeof(nor));for(i=0;i<APP_LIMIT;i++) {((uint8_t*)APP_BASE)[i]=(uint8_t)(i*13+7);}
    put32((uint8_t*)APP_BASE,0x20005000);put32((uint8_t*)APP_BASE+4,APP_BASE+9);memcpy(old_flash,(void*)APP_BASE,APP_LIMIT);
    active.version=0x10000;active.size=777;active.crc32=CRC32_IEEE((void*)APP_BASE,777);
    C(SpiBus_Init());C(W25_Init(&port));C(Meta_Provision(&active));C(Meta_Scan(&scan));
    prepare_guards(&active);
    record=scan.record;record.state=RECEIVING;C(Meta_Commit(&record));
    for(i=32;i<sizeof(new_file);i++) {new_file[i]=(uint8_t)i;}
    put32(new_file+32,0x20005000);put32(new_file+36,APP_BASE+9);
    new_id.version=0x20000;new_id.size=sizeof(new_file)-32;new_id.crc32=CRC32_IEEE(new_file+32,new_id.size);
    put32(new_file,0x46574D47);put32(new_file+4,1);put32(new_file+8,new_id.version);put32(new_file+12,new_id.size);put32(new_file+16,new_id.crc32);
    put32(new_file+20,APP_BASE);put32(new_file+24,0);put32(new_file+28,CRC32_IEEE(new_file,28));
    if(mode==2) {C(Log_Append(LOG_START_DOWNLOAD,RECEIVING,1,0,0));C(Log_Append(LOG_ERROR,RECEIVING,1,1,0));}
    if(mode==3) {BootProtocol_Init();C(!(model_rx.CCR&DMA_CCR_EN));C(!(model_uart.CR3&USART_CR3_DMAR));}
    Product_EarlyInit();if(!setjmp(finish_test)) {Product_Main();}
    C(mode==0?jumps==1:jumps==0);
    printf("{\"result\":\"PASS\",\"mode\":%u,\"assertions\":%u,\"flash_ops\":%u,\"jumps\":%u}\n",mode,checks,flash_ops,jumps);return 0;
}

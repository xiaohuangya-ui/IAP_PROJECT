#include "ymodem.h"
#include "crc16.h"
#include <string.h>
#define SOH 1U
#define STX 2U
#define EOT 4U
#define ACK 6U
#define NAK 21U
#define CAN 24U
#define CRC_REQUEST 67U
#ifdef PROTOCOL_PRODUCT_BINDING
#include "iap_fsm.h"
#include "uart_port.h"
#include "stm32f1xx_hal.h"
static uint32_t now(YModem *y){(void)y;return HAL_GetTick();}
#define sink_abort(y) Iap_Abort()
#define sink_tx(y,p,n) Uart_TxSubmit(p,n)
#define sink_begin(y,n) Iap_Begin(n)
#define sink_write(y,o,p,n) Iap_Write(o,p,n)
#define sink_finish(y,i) Iap_Finish(i)
#else
static uint32_t now(YModem *y){return y->port.clock(y->port.context);}
#define sink_abort(y) (y)->port.abort((y)->port.context)
#define sink_tx(y,p,n) (y)->port.tx((y)->port.context,p,n)
#define sink_begin(y,n) (y)->port.begin((y)->port.context,n)
#define sink_write(y,o,p,n) (y)->port.write((y)->port.context,o,p,n)
#define sink_finish(y,i) (y)->port.finish((y)->port.context,i)
#endif
static int active(const YModem *y){return y->state>=YM_BLOCK0 && y->state<=YM_FINAL;}
static void send(YModem *y,uint8_t a,uint8_t b)
{
    y->response[0]=a;y->response[1]=b;y->response_length=b?2U:1U;y->tx_since=now(y);
}
static void fail(YModem *y,YmError error,YmState state)
{
    /* 一次abort使新下载无效；永不调用Flash/NOR/BKP/IWDG。 */
    if(y->begun){sink_abort(y);y->begun=0U;}
    y->error=error;y->state=state;y->used=0U;send(y,CAN,CAN);
}
static void retry(YModem *y,YmError error)
{
    y->used=0U;y->cancel=0U;y->wait_since=now(y);
    if(++y->retries>=YM_MAX_CONSECUTIVE_ERRORS){fail(y,error,YM_ERROR);}
    else {send(y,NAK,0U);} /* 损坏/截断帧统一NAK；无包启动窗口另发C。 */
}
#ifdef PROTOCOL_PRODUCT_BINDING
/* 产品固定sink必须显式启动；自定义端口在此配置拒绝，绝不静默替换。 */
int YModem_Start(YModem *y,const YmPort *port){(void)y;(void)port;return 0;}
int YModem_StartProduct(YModem *y)
{
    if(!y){return 0;}
    memset(y,0,sizeof(*y));y->state=YM_BLOCK0;y->expected=1U;
    y->image_crc=0xFFFFFFFFU;y->start=y->wait_since=now(y);send(y,CRC_REQUEST,0U);return 1;
}
#else
int YModem_Start(YModem *y,const YmPort *port)
{
    if(!y || !port || !port->begin || !port->write || !port->finish || !port->abort || !port->tx || !port->clock){return 0;}
    memset(y,0,sizeof(*y));y->port=*port;y->state=YM_BLOCK0;y->expected=1U;
    y->image_crc=0xFFFFFFFFU;y->start=y->wait_since=now(y);send(y,CRC_REQUEST,0U);return 1;
}
#endif
int YModem_CanRead(const YModem *y){return (active(y) || (y->state==YM_DONE && !y->closed)) && !y->response_length;}
void YModem_Poll(YModem *y)
{
    uint32_t t=now(y);
    if(y->response_length) {
        if(sink_tx(y,y->response,y->response_length)) {
            y->response_length=0U;y->wait_since=now(y);
        } else if((uint32_t)(t-y->tx_since)>=YM_PACKET_TIMEOUT_MS) {
            if(active(y)){fail(y,YM_ERR_TX,YM_ERROR);}
            y->response_length=0U; /* TX坏不能无界重发CAN。 */
        }
        return;
    }
    /* 最终ACK丢失可重发，但重包不能无限延长模式独占；从首次完成计2s。 */
    if(y->state==YM_DONE && (uint32_t)(t-y->completed_since)>=YM_FINAL_ACK_GRACE_MS){y->closed=1U;y->used=0U;}
    if(!active(y)){return;}
    if(y->state==YM_BLOCK0 && (uint32_t)(t-y->start)>=YM_HANDSHAKE_TIMEOUT_MS){fail(y,YM_ERR_TIMEOUT,YM_ERROR);return;}
    if((uint32_t)(t-y->wait_since)>=YM_PACKET_TIMEOUT_MS){
        if(y->state==YM_BLOCK0 && !y->used){send(y,CRC_REQUEST,0U);}
        else {retry(y,YM_ERR_TIMEOUT);}
    }
}
static int block0_size(const uint8_t *p,uint16_t length,uint32_t *size)
{
    uint32_t i=0U,start,n=0U;
    /* 单文件名1..63B，可打印无路径；大小有界十进制允许前导0，后可跟mtime。
     * 累积前检查最大值，既不溢出，也不把合法前导0误判成超长整数。 */
    while(i<length && p[i]){if(i>=63U || (uint32_t)(p[i]-32U)>94U || p[i]=='/' || p[i]=='\\'){return 0;}i++;}
    if(i==0U || i>=length){return 0;}i++;start=i;
    while(i<length && (uint32_t)(p[i]-'0')<=9U) {
        uint32_t digit=(uint32_t)(p[i]-'0');
        /* n<=4918时乘10加一位仍不溢出；再检查相同49184上界。
         * 避免每位一次UDIV，保留前导0、mtime及所有原拒绝条件。 */
        if(n>4918U){return 0;}n=n*10U+digit;if(n>49184U){return 0;}i++;
    }
    if(i==start || i>=length || (p[i]!=0U && p[i]!=' ') || n<33U || n>49184U){return 0;}
    *size=n;return 1;
}
static void packet(YModem *y)
{
    uint16_t length=(uint16_t)(y->needed-5U),count,crc;uint32_t j;
    uint8_t id=y->packet[1];const uint8_t *p=y->packet+3;
    crc=(uint16_t)((uint16_t)p[length]<<8|p[length+1U]);
    y->used=0U;
    if((uint8_t)(id^y->packet[2])!=255U || CRC16_Xmodem(p,length)!=crc){retry(y,YM_ERR_PACKET);return;}
    if((uint32_t)((uint32_t)y->state-YM_FINAL)<=YM_DONE-YM_FINAL) {
        if(id!=0U){retry(y,YM_ERR_PACKET);return;}
        for(j=0U;j<length;j++){if(p[j]!=0U){fail(y,YM_ERR_LENGTH,YM_ERROR);return;}}
        /* 协议结束仍需sink完整验证；此结果从不直接标记持久download_valid。 */
        if(y->state!=YM_DONE && !sink_finish(y,&y->identity)){fail(y,YM_ERR_SINK,YM_ERROR);return;}
        if(y->state!=YM_DONE){y->completed_since=now(y);}
        y->begun=0U;y->state=YM_DONE;send(y,ACK,0U);return;
    }
    if(y->state==YM_BLOCK0) {
        if(id!=0U || !block0_size(p,length,&y->file_size)){fail(y,YM_ERR_LENGTH,YM_ERROR);return;}
        if(!sink_begin(y,y->file_size)){fail(y,YM_ERR_SINK,YM_ERROR);return;}
        y->begun=1U;y->state=YM_DATA;y->expected=1U;
    } else if(y->have_previous && id==y->previous_id) {
        if(length!=y->previous_length || memcmp(p,y->previous,length)) {retry(y,YM_ERR_PACKET);return;}
        /* 丢ACK的重传只ACK，不推进offset/CRC。重复Block0需重发ACK+C。 */
        y->retries=0U;send(y,ACK,y->received==0U?CRC_REQUEST:0U);return;
    } else {
        if(y->state!=YM_DATA || id!=y->expected || y->received>=y->file_size){retry(y,YM_ERR_PACKET);return;}
        count=(uint16_t)((y->file_size-y->received)<length?(y->file_size-y->received):length);
        if(y->received==0U) {
            if(!FirmwareHeader_Decode(p,count,y->file_size,&y->identity)){fail(y,YM_ERR_HEADER,YM_ERROR);return;}
            if(y->identity.size<8U){fail(y,YM_ERR_IMAGE,YM_ERROR);return;}
            memcpy(y->vector,p+32U,8U);
        }
        /* 末包padding不参与CRC，也不交给sink。Header首次校验先于介质接口。 */
        j=y->received==0U?32U:0U;
        if(!sink_write(y,y->received,p,count)){fail(y,YM_ERR_SINK,YM_ERROR);return;}
        y->image_crc=CRC32_Update(y->image_crc,p+j,(uint32_t)(count-j));
        y->received+=count;y->expected=(uint8_t)(y->expected+1U);
    }
    memcpy(y->previous,p,length);y->previous_length=length;y->previous_id=id;y->have_previous=1U;
    y->retries=0U;y->wait_since=now(y);send(y,ACK,y->received==0U?CRC_REQUEST:0U);
}
void YModem_Feed(YModem *y,uint8_t byte)
{
    if(!YModem_CanRead(y)){return;}
    if(y->used) {
        y->packet[y->used++]=byte;
        if(y->used==y->needed){packet(y);}return;
    }
    if(byte==CAN) {
        if(y->cancel){fail(y,YM_ERR_CANCEL,YM_CANCELLED);}else {y->cancel=1U;}return;
    }
    y->cancel=0U;
    if(byte==SOH || byte==STX) {y->packet[0]=byte;y->used=1U;y->needed=byte==SOH?133U:1029U;return;}
    if(byte==EOT) {
        if(y->state==YM_FINAL){send(y,ACK,CRC_REQUEST);return;} /* 丢第二EOT ACK。 */
        if(y->state==YM_EOT2){y->state=YM_FINAL;send(y,ACK,CRC_REQUEST);return;}
        if(y->state!=YM_DATA || y->received!=y->file_size){fail(y,YM_ERR_LENGTH,YM_ERROR);return;}
        if((y->image_crc^0xFFFFFFFFU)!=y->identity.crc32 ||
           !AppVector_Valid(y->identity.size,Wire_ReadLE32(y->vector),Wire_ReadLE32(y->vector+4U))) {
            fail(y,YM_ERR_IMAGE,YM_ERROR);return;
        }
        y->state=YM_EOT2;y->retries=0U;send(y,NAK,0U);return;
    }
    if(y->state!=YM_DONE){retry(y,YM_ERR_PACKET);}
}
void YModem_UartFault(YModem *y){if(active(y) || (y->state==YM_DONE && !y->closed)){fail(y,YM_ERR_UART,YM_ERROR);}}

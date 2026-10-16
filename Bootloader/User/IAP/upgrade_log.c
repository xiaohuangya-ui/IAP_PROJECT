/* Boot日志owner：只经Store访问Log分区，前台同步调用；无ISR/重入。
 * 日志不是恢复权威，失败不能撤销已成功的Metadata提交。 */
#include "byte_buffer.h"
#include "upgrade_log.h"
#include "store.h"
#include "fw_header.h"
#include <string.h>
#include <stddef.h>
#define blank(b) Bytes_AllFF(b,32U)
static int decode(const uint8_t *b,LogRecord *r)
{
    if(Wire_ReadLE32(b)!=0x4C4F4731U||Wire_ReadLE32(b+28)!=CRC32_IEEE(b,28U)){return 0;}
    /* 固定六个标量显式解码，继续不依赖结构体padding或原样序列化。 */
    r->sequence=Wire_ReadLE32(b+4U);r->event=Wire_ReadLE32(b+8U);r->state=Wire_ReadLE32(b+12U);
    r->version=Wire_ReadLE32(b+16U);r->error=Wire_ReadLE32(b+20U);r->detail=Wire_ReadLE32(b+24U);
    return r->sequence&&r->sequence<0xFFFFFFFEU&&r->event>=LOG_START_DOWNLOAD&&r->event<=LOG_POWERLOSS_RECOVERY&&r->state>=1U&&r->state<=9U;
}
int Log_ReadNext(uint32_t *cursor,LogRecord *r,uint32_t *corrupt)
{
    uint8_t b[32];
    if(!cursor||!r||!corrupt){return -1;}
    while(*cursor<512U){
        if(!Store_Read(STORE_LOG,(*cursor)++*32U,b,32U)){return -1;}
        if(blank(b)){continue;}if(decode(b,r)){return 1;}++*corrupt;
    }return 0;
}
int Log_Append(LogEvent event,uint32_t state,uint32_t version,uint32_t error,uint32_t detail)
{
    uint8_t b[32],verify[32];uint32_t i,slot=0,seq=0,w[8],free_slot=512U;LogRecord r;
    if(event<LOG_START_DOWNLOAD||event>LOG_POWERLOSS_RECOVERY||state<1U||state>9U){return 0;}
    for(i=0;i<512U;++i){
        if(!Store_Read(STORE_LOG,i*32U,b,32U)){return 0;}
        if(blank(b)&&free_slot==512U){free_slot=i;}
        if(decode(b,&r)&&r.sequence>seq){seq=r.sequence;slot=i;}
    }
    if(seq>=0xFFFFFFFDU){return 0;}
    if(!seq){if(free_slot==512U){return 0;}slot=free_slot;}
    else{
        slot=(slot+1U)%512U;
        if(!Store_Read(STORE_LOG,slot*32U,b,32U)){return 0;}
        if(!blank(b)){
            /* 脏槽不能改写。只回收下一扇区，永不擦当前最新事件所在扇区。 */
            /* 128槽/扇区；边界槽保留，非边界向上取整到下一扇区，再环回。
             * 保留已验证的现行表达式；NOR掉电/轮转硬件仍OPEN。 */
            slot=((slot/128U)+(slot%128U?1U:0U))*128U;slot%=512U;
            if(!Store_Erase(STORE_LOG,slot*32U,4096U)){return 0;}
        }
    }
    w[0]=0x4C4F4731U;w[1]=seq+1U;w[2]=event;w[3]=state;w[4]=version;w[5]=error;w[6]=detail;
    /* 字段表/CRC末写语义不变；共用标量LE原语，保留独立w暂存和回读。 */
#if defined(__CC_ARM) && !defined(__BIG_ENDIAN)
    /* uint32_t标量数组在LE目标上的字节表示就是wire顺序；不是结构体序列化。
     * 同一独立w暂存，CRC与末写/回读顺序不变。 */
    memcpy(b,w,28U);
#else
    for(i=0;i<7U;++i){Wire_WriteLE32(b+4U*i,w[i]);}
#endif
    w[7]=CRC32_IEEE(b,28U);Wire_WriteLE32(b+28U,w[7]);
    return Store_Program(STORE_LOG,slot*32U,b,28U)&&Store_Program(STORE_LOG,slot*32U+28U,b+28,4U)&&
        Store_Read(STORE_LOG,slot*32U,verify,32U)&&!memcmp(b,verify,32U);
}

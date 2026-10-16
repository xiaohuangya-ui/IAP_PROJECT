#include "byte_buffer.h"
#include "metadata.h"
#include "internal_flash.h"
#include <string.h>
#include <stddef.h>
/* 显式on-flash字段顺序→本机成员偏移；不是结构体原样序列化。
 * 每项都指向真实uint32_t子对象；编译器计算padding，wire始终逐字节LE。 */
static const uint8_t fields[]={
    offsetof(Metadata,sequence),offsetof(Metadata,state),
    offsetof(Metadata,active.version),offsetof(Metadata,active.size),offsetof(Metadata,active.crc32),
    offsetof(Metadata,pending.version),offsetof(Metadata,pending.size),offsetof(Metadata,pending.crc32),
    offsetof(Metadata,backup.image.version),offsetof(Metadata,backup.physical_crc),
    offsetof(Metadata,backup.image.size),offsetof(Metadata,backup.image.crc32),
    offsetof(Metadata,download_valid),offsetof(Metadata,backup_valid),offsetof(Metadata,attempts),offsetof(Metadata,error)};
typedef char MetadataOffsetsFit[(sizeof(Metadata)<256U)?1:-1];
static int identity(const ImageIdentity *i)
{return i->version && (uint32_t)(i->size-8U)<=APP_LIMIT-8U;}
static int equal(const ImageIdentity *a,const ImageIdentity *b)
{return a->version==b->version&&a->size==b->size&&a->crc32==b->crc32;}
int Meta_Valid(const Metadata *r)
{
    /* 每态只描述纯字段约束；不替代介质读取/CRC/transition或提交后扫描。
     * A=Active合法；P=Pending与版本合法；D/B=必须valid；N=禁止Download；
     * T=允许attempts；E=Backup有效时必须匹配Active。先验证状态再索引。 */
    enum {A=1,P=2,D=4,B=8,N=16,T=32,E=64};
    static const uint8_t requirements[]={A,A|N,A|P,A|P,A|D|E,A|D|B|E,A|D|B|T|E,A|B|E,0};
    uint32_t flags;
    if(!r||!r->sequence||(uint32_t)(r->state-CONFIRMED)>FAILED-CONFIRMED||
       r->download_valid>1U||r->backup_valid>1U||r->attempts>3U){return 0;}
    flags=requirements[r->state-CONFIRMED];
    if((flags&A)&&!identity(&r->active)){return 0;}
    if(!(flags&T)&&r->attempts){return 0;}
    if((r->download_valid||(flags&P))&&
       (!identity(&r->pending)||r->pending.version<r->active.version)){return 0;}
    if(r->backup_valid&&!identity(&r->backup.image)){return 0;}
    if(((flags&D)&&!r->download_valid)||((flags&B)&&!r->backup_valid)||
       ((flags&N)&&r->download_valid)){return 0;}
    if(r->backup_valid&&(flags&E)&&!equal(&r->active,&r->backup.image)){return 0;}
    return 1;
}
static int transition(const Metadata *a,const Metadata *b)
{
    /* bit0..8对应来源CONFIRMED..FAILED；先验证范围再移位。
     * 表只替代边集合，身份、attempts及Confirm/Rollback特例仍独立核验。 */
    static const uint16_t from[]={1U,3U,2U,12U,24U,48U,96U,224U,0U};
    if(b->state==FAILED){return equal(&a->active,&b->active);}
    if(b->state==CONFIRMED){
        if(a->state==PENDING_CONFIRM){return equal(&b->active,&a->pending)&&a->attempts!=0U;}
        if(a->state==ROLLBACK){return equal(&b->active,&a->backup.image);}
    }
    if((uint32_t)(a->state-CONFIRMED)>FAILED-CONFIRMED||(uint32_t)(b->state-CONFIRMED)>FAILED-CONFIRMED||
       !(from[b->state-1U]&(1U<<(a->state-1U)))||!equal(&a->active,&b->active)){return 0;}
    if(b->state==PENDING_CONFIRM&&
       !((a->state==INSTALLING&&b->attempts==0U)||
         (a->state==PENDING_CONFIRM&&b->attempts==a->attempts+1U))){return 0;}
    if(b->state>=VERIFYING&&b->state<=ROLLBACK&&!equal(&a->pending,&b->pending)){return 0;}
    return 1;
}
void Meta_Encode(const Metadata *r,uint8_t out[72])
{
    uint32_t i;Wire_WriteLE32(out,0x4D455432U);
    for(i=0;i<16U;++i){Wire_WriteLE32(out+4U+4U*i,*(const uint32_t *)((const uint8_t *)r+fields[i]));}
    Wire_WriteLE32(out+68,CRC32_IEEE(out,68U));
}
int Meta_Decode(const uint8_t in[72],Metadata *r)
{
    uint32_t i;
    if(Wire_ReadLE32(in)!=0x4D455432U||Wire_ReadLE32(in+68)!=CRC32_IEEE(in,68U)){return 0;}
    for(i=0;i<16U;++i){*(uint32_t *)((uint8_t *)r+fields[i])=Wire_ReadLE32(in+4U+4U*i);}
    return Meta_Valid(r);
}
int Meta_Scan(MetaScan *s)
{
    /* 112槽的序号/位置缓存只在单前台Scan中使用；无介质副作用。 */
    static uint32_t seq[112];static uint8_t loc[112];
    uint32_t area,slot,count=0,j;uint8_t b[72],old[72];Metadata r;
    memset(s,0,sizeof(*s));
    for(area=0;area<2U;++area){for(slot=0;slot<56U;++slot){
        if(!Store_Read((StoreArea)(STORE_META_A+area),slot*72U,b,72U)){return 0;}
        if(Bytes_AllFF(b,72U)){continue;}
        if(!Meta_Decode(b,&r)){s->dirty++;continue;}
        for(j=0;j<count;++j){if(seq[j]==r.sequence){
            if(!Store_Read((StoreArea)(STORE_META_A+(loc[j]>>6)),(loc[j]&63U)*72U,old,72U)||memcmp(b,old,72U)){return 0;}
        }}
        /* 私有缓存编码：bit6=扇区，bit0..5=槽；介质地址/112项容量不变。
         * 避免冲突复读路径对56的除余，所有读次序保持原样。 */
        seq[count]=r.sequence;loc[count]=(uint8_t)((area<<6)|slot);++count;
        if(!s->found||r.sequence>s->record.sequence){s->found=1;s->record=r;s->area=area;s->slot=slot;}
    }}
    return 1;
}
static int append(uint32_t area,uint32_t slot,const Metadata *r)
{
    uint8_t b[72],read[72];Metadata check;StoreArea a=(StoreArea)(STORE_META_A+area);
    if(!Store_Read(a,slot*72U,read,72U)||!Bytes_AllFF(read,72U)){return 0;}
    Meta_Encode(r,b);
    return Store_Program(a,slot*72U,b,68U)&&Store_Program(a,slot*72U+68U,b+68,4U)&&
        Store_Read(a,slot*72U,read,72U)&&!memcmp(b,read,72U)&&Meta_Decode(read,&check);
}
int Meta_Commit(Metadata *r)
{
    MetaScan s;Metadata next;uint8_t b[72];uint32_t slot,area;
    if(!r||!Meta_Valid(r)||!Meta_Scan(&s)||!s.found||r->sequence!=s.record.sequence||r->sequence>=0xFFFFFFFEU){return 0;}
    if(!transition(&s.record,r)){return 0;}
    next=*r;next.sequence++;area=s.area;
    /* 只在权威之后寻找完整空白槽；DIRTY不能修补/覆盖。 */
    for(slot=s.slot+1U;slot<56U;++slot){
        if(!Store_Read((StoreArea)(STORE_META_A+area),slot*72U,b,72U)){return 0;}
        if(Bytes_AllFF(b,72U)){break;}
    }
    if(slot==56U){
        area^=1U;slot=0;
        /* 已确认旧权威位于另一区，备用擦除失败不得动旧权威。 */
        if(!Store_Erase((StoreArea)(STORE_META_A+area),0,4096U)){return 0;}
    }
    if(!append(area,slot,&next)){return 0;}
    /* 不立即擦旧区：下次轮转才在另一区保有权威的条件下回收。 */
    if(!Meta_Scan(&s)||!s.found||s.record.sequence!=next.sequence){return 0;}
    *r=next;return 1;
}
#ifdef IAP_PROVISION_TEST_ONLY
int Meta_Provision(const ImageIdentity *active)
{
    Metadata r;MetaScan s;uint8_t b[128];uint32_t a,o;
    if(!InternalFlash_Verify(active)||!Meta_Scan(&s)||s.found){return 0;}
    for(a=0;a<2U;++a){for(o=0;o<4096U;o+=sizeof(b)){
        if(!Store_Read((StoreArea)(STORE_META_A+a),o,b,sizeof(b))||!Bytes_AllFF(b,sizeof(b))){return 0;}
    }}
    memset(&r,0,sizeof(r));r.sequence=1;r.state=CONFIRMED;r.active=*active;
    return append(0,0,&r);
}
#endif

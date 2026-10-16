#include "byte_buffer.h"
#include "internal_flash_port.h"
#ifdef IAP_PHASE8_ENABLED
#include "boot_iwdg.h"
#endif
static int range(uint32_t a,uint32_t n)
{return n && a-APP_BASE<APP_LIMIT && n<=APP_LIMIT-(a-APP_BASE);}
int InternalFlash_Read(uint32_t a,uint8_t *p,uint32_t n)
{return p && range(a,n) && FlashPort_Read(a,p,n);}
FlashResult InternalFlash_Erase(uint32_t a,uint32_t n)
{
    uint32_t i;uint8_t b[32];FlashResult r;
    if(!range(a,n)){return IF_RANGE;}
    if((a&1023U)||(n&1023U)){return IF_ALIGN;}
    if(!FlashPort_Context()){return IF_CONTEXT;}
    while(n){
        r=FlashPort_Operation(a,0xFFFFU,1);if(r!=IF_OK){return r;}
        for(i=0;i<1024U;i+=sizeof(b)){
            if(!FlashPort_Read(a+i,b,sizeof(b))){return IF_VERIFY;}
            if(!Bytes_AllFF(b,sizeof(b))){return IF_VERIFY;}
        }
        a+=1024U;n-=1024U;
#ifdef IAP_PHASE8_ENABLED
        Iwdg_Progress(); /* 一页已完成且1024B回读通过，轮询忙状态不喂。 */
#endif
    }
    return IF_OK;
}
FlashResult InternalFlash_Program(uint32_t a,const uint8_t *p,uint32_t n)
{
    uint32_t i;uint16_t value;uint8_t b[2];FlashResult r;
    if(!p || !range(a,n) || (n&1U && !range(a,n+1U))){return IF_RANGE;}
    if(a&1U){return IF_ALIGN;}
    if(!FlashPort_Context()){return IF_CONTEXT;}
    /* 1. 全范围先检查擦除态，防止中途才发现旧数据。 */
    for(i=0;i<n;i+=2U){
        if(!FlashPort_Read(a+i,b,2U)||b[0]!=0xFFU||b[1]!=0xFFU){return IF_PROGRAM;}
    }
    /* 2. 每半字完成后回读；任何失败立即停止，事务不得标有效。 */
    for(i=0;i<n;i+=2U){
        value=(uint16_t)(p[i]|((uint16_t)(i+1U<n?p[i+1U]:0xFFU)<<8));
        if(value!=0xFFFFU){
            r=FlashPort_Operation(a+i,value,0);if(r!=IF_OK){return r;}
        }
        if(!FlashPort_Read(a+i,b,2U)||b[0]!=(uint8_t)value||b[1]!=(uint8_t)(value>>8)){return IF_VERIFY;}
    }
#ifdef IAP_PHASE8_ENABLED
    Iwdg_Progress(); /* 调用方按256B提交，完成后才有进展资格。 */
#endif
    return IF_OK;
}
int InternalFlash_Verify(const ImageIdentity *id)
{
    if(!id||!id->version||(uint32_t)(id->size-8U)>APP_LIMIT-8U){return 0;}
    /* 每次调用仍独立读取当前内部Flash的向量和全部有效字节。
     * 内部Flash是连续CPU映射，复用Jump使用的纯验证器，省去128B复制栈。
     * 不合并安装/Trial/Confirm/Jump不同时间点的必要复验。 */
    return Image_Validate(FlashPort_View(APP_BASE),APP_LIMIT,id);
}

/* 保留全部旧oracle；另遍历Trial/Confirm/Rollback每个持久提交字节失败。
 * 真实FSM/Metadata/Log/Flash实现，介质与Flash寄存器为模型。 */
#define main legacy_main
#include "transaction_tests.c"
#undef main
static uint8_t pending_medium[sizeof(medium)],pending_flash[sizeof(flash)];
static void dump(unsigned op,unsigned point,int result)
{
    MetaScan s;int found=Meta_Scan(&s);
    printf("%u %u %d %d %08lx %08lx ",op,point,result,found,
           (unsigned long)CRC32_IEEE(medium,sizeof(medium)),(unsigned long)CRC32_IEEE(flash,sizeof(flash)));
    if(found&&s.found){printf("%lu %lu %lu\n",(unsigned long)s.record.state,(unsigned long)s.record.attempts,(unsigned long)s.record.sequence);}
    else{puts("NONE");}
}
int main(void)
{
    unsigned op,i,point;int result;
    C(!legacy_main());seed();download();C(Iap_Recover());C(Iap_TrialCommit());
    memcpy(pending_medium,medium,sizeof(medium));memcpy(pending_flash,flash,sizeof(flash));
    for(op=0;op<3;++op)for(i=0;i<220;++i){
        memcpy(medium,pending_medium,sizeof(medium));memcpy(flash,pending_flash,sizeof(flash));cut=-1;C(Iap_Load());
        /* start Journal+Log=104，48页+24576半字，最后Journal+Log=104。
         * 覆盖两个提交的每个字节，及Flash的页/半字首尾拒绝。 */
        point=i<106?i:104U+48U+24576U+i-106U;cut=point;
        result=op==0?Iap_TrialCommit():op==1?Iap_Confirm():Iap_Rollback();cut=-1;dump(op,point,result);
        C(Iap_Load()); /* 模拟掉电重载，不依赖已失效RAM快照。 */
        if(Iap_Record()->state==ROLLBACK){(void)Iap_Recover();}
        dump(op,point,1);
    }
    return 0;
}

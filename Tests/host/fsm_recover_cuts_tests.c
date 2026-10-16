/* 真实恢复链的六次Journal提交：逐字节写裂、重载、再次恢复。保留旧oracle。 */
#define Store_Program Medium_Program
#define main legacy_main
#include "transaction_tests.c"
#undef main
#undef Store_Program
static unsigned commit_index,target_commit,target_byte,enabled,triggered;
static uint8_t start_medium[sizeof(medium)],start_flash[sizeof(flash)];
int Store_Program(StoreArea a,uint32_t o,const uint8_t *p,uint32_t n)
{
    long previous=cut;int result,injected=0;
    if(enabled&&(a==STORE_META_A||a==STORE_META_B)){
        if(n==68U){++commit_index;}
        if(commit_index==target_commit&&((n==68U&&target_byte<68U)||(n==4U&&target_byte>=68U))){
            cut=(long)(target_byte-(n==4U?68U:0U));++triggered;injected=1;
        }
    }
    result=Medium_Program(a,o,p,n);if(injected){cut=previous;}return result;
}
int main(void)
{
    ImageIdentity id;unsigned commit,byte;MetaScan s;
    C(!legacy_main());seed();id=download();
    memcpy(start_medium,medium,sizeof(medium));memcpy(start_flash,flash,sizeof(flash));
    enabled=1;target_commit=99;commit_index=0;C(Iap_Recover());C(commit_index==6);enabled=0;
    for(commit=1;commit<=6;++commit)for(byte=0;byte<72;++byte){
        memcpy(medium,start_medium,sizeof(medium));memcpy(flash,start_flash,sizeof(flash));C(Iap_Load());
        target_commit=commit;target_byte=byte;commit_index=triggered=0;enabled=1;
        C(!Iap_Recover());C(triggered==1);enabled=0;
        C(Meta_Scan(&s)&&s.found);
        printf("CUT %u %u state=%lu seq=%lu flash=%08lx medium=%08lx\n",commit,byte,
            (unsigned long)s.record.state,(unsigned long)s.record.sequence,
            (unsigned long)CRC32_IEEE(flash,sizeof(flash)),(unsigned long)CRC32_IEEE(medium,sizeof(medium)));
        C(Iap_Load());C(Iap_Recover());C(Iap_Record()->state==PENDING_CONFIRM);
        C(InternalFlash_Verify(&id));C(Backup_Verify(&Iap_Record()->backup));
    }
    printf("RECOVERY CUTS PASS cases=432 checks=%u\n",checks);return 0;
}

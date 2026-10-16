/* 原oracle原样执行，再扩展明确的PSP产品入口；不改写旧期望值。 */
#define main legacy_main
#include "app_bkp_tests.c"
#undef main
int main(void)
{
    unsigned control,mask;uint32_t before;
    CHECK(legacy_main()==0);host_ipsr=0;
    for(control=0;control<4;++control){
        host_control=control;before=BKP->DR1=0x1234;
        if(control==2){CHECK(Backup_AppTaskRequest(1)==1 && BKP->DR1==0x55AA);}
        else{CHECK(Backup_AppTaskRequest(1)==0 && BKP->DR1==before);}
    }
    host_control=2;
    for(mask=0;mask<4;++mask){
        host_ipsr=mask==0;host_primask=mask==1;host_basepri=mask==2?0x50:0;host_faultmask=mask==3;
        before=BKP->DR2=0x4321;CHECK(!Backup_AppTaskRequest(2) && BKP->DR2==before);
    }
    host_ipsr=host_primask=host_basepri=host_faultmask=0;
    CHECK(Backup_AppTaskRequest(2)==1 && BKP->DR2==0x5AA5);
    CHECK(!Backup_AppTaskRequest(0) && !Backup_AppTaskRequest(3));
    puts("PSP product task guards PASS; legacy MSP oracle retained");return 0;
}

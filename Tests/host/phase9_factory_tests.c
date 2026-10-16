#define main legacy_main
#include "transaction_tests.c"
#undef main
#include "../../hardware/phase9_factory.c"
static uint8_t output9[200000],original9[sizeof(medium)];
static unsigned output_used9,input_used9;static const uint8_t *input9;
static int fail_tc9;
uint32_t HAL_GetTick(void){return 0;}
int Uart_Start(void){return 1;}
void Uart_Service(void){}
int Uart_TxSubmit(const uint8_t *p,uint16_t n){C(output_used9+n<=sizeof(output9));memcpy(output9+output_used9,p,n);output_used9+=n;return 1;}
int Uart_FlushTC(uint32_t ms){C(ms==100U);return !fail_tc9;}
uint32_t Uart_TakeErrors(void){return 0;}
int Uart_ReadByte(uint8_t *p){if(input9 && input_used9<8192U){*p=input9[input_used9++];return 1;}return 0;}
int Uart_PauseReceive(void){return 1;}
int Uart_ResumeReceive(void){return 1;}
int SpiBus_Init(void){return 1;}
int W25_Init(const W25Port *p){C(p && p->progress && p->service && p->clock_ms);return 1;}
int W25_Read(uint32_t a,uint8_t *p,uint32_t n){C(a<=sizeof(medium) && n<=sizeof(medium)-a);memcpy(p,medium+a,n);return 1;}
int main(int argc,char **argv)
{
    unsigned i,changes;char request9[128];MetaScan scan;FILE *file9;
    C(argc==2);memset(flash,255,sizeof(flash));file9=fopen(argv[1],"rb");C(file9);C(fread(flash,1,PHASE9_V1_SIZE,file9)==PHASE9_V1_SIZE);fclose(file9);
    for(i=0;i<sizeof(medium);++i){medium[i]=(uint8_t)(i*17U+(i>>8));}memcpy(original9,medium,sizeof(medium));cut=-1;
    changes=medium_changes;C(command("@FACTORY:PROVISION_V1:00000000"));C(medium_changes==changes);
    output_used9=0;C(command("@FACTORY:BACKUP"));C(backup_sent);C(!memcmp(output9+strlen("FACTORY DATA 155648\r\n"),original9,sizeof(medium)));
    (void)snprintf(request9,sizeof(request9),"@FACTORY:PROVISION_V1:%08lX",(unsigned long)backup_crc);
    flash[100]^=1;C(command(request9));C(medium_changes==changes);flash[100]^=1;
    C(command("@FACTORY:PROVISION_V1:FFFFFFFF"));C(medium_changes==changes);
    C(command(request9));C(Meta_Scan(&scan) && scan.found && scan.record.state==CONFIRMED && scan.record.active.crc32==PHASE9_V1_CRC);
    C(!memcmp(medium,original9,0x20000));C(!memcmp(medium+0x22000,original9+0x22000,0x4000));C(!backup_sent && !locked);
    changes=medium_changes;C(command(request9));C(medium_changes==changes);
    output_used9=0;C(command("@FACTORY:BACKUP"));input9=original9+0x20000;input_used9=0;
    (void)snprintf(request9,sizeof(request9),"@FACTORY:RESTORE_META:%08lX",(unsigned long)CRC32_IEEE(input9,8192));
    output_used9=0;C(command(request9));C(input_used9==8192U);C(!memcmp(medium,original9,sizeof(medium)));
    output_used9=0;fail_tc9=1;C(!command("@FACTORY:BACKUP"));C(!backup_sent);fail_tc9=0;
    printf("{\"result\":\"PASS\",\"checks\":%u,\"mutable_backup_bytes\":155648,\"metadata_restore_bytes\":8192,\"other_regions_unchanged\":true}\n",checks);return 0;
}

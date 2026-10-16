"""Connect real Product/Protocol/FSM/Journal/Store/W25/SPI/BKP/Flash/Jump Prepare.

Only hardware medium and CPU boundary are modeled; no device access.
"""
from pathlib import Path
import sys,subprocess,json,hashlib,shutil
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
# One consistent register model: extend existing SPI/BKP fixture with LL USART/DMA.
h=(R/'Tests/host/stubs/stm32f1xx.h').read_text(encoding='utf-8')
h=h[:h.index('typedef struct { volatile uint32_t SR; } USART_TypeDef;')]
a=h.index('extern uint32_t host_ipsr');b=h.index('#define __DSB()',a)
h=h[:a]+'''extern uint32_t host_control;
#define __get_CONTROL() host_control
'''+h[b:]
h+='''typedef struct { volatile uint32_t IABR[2]; } NVIC_TypeDef;
extern NVIC_TypeDef host_nvic;
#define NVIC (&host_nvic)
#define RCC_CR_HSIRDY (1U<<1)
#include "ll_device_model.h"
#endif
'''
(O/'stm32f1xx.h').write_text(h)
for n in ['spi.h','main.h']:
    shutil.copy2(R/'Tests/host/stubs'/n,O/n)
(O/'stm32f1xx_hal.h').write_text('#include "stm32f1xx.h"\n#define __declspec(x) __attribute__((x))\nuint32_t HAL_GetTick(void);\n')
prefix=(R/'Bootloader/User/IAP/jump_ll.c').read_text(encoding='utf-8').split('/* AC5内嵌汇编函数',1)[0]
(O/'jump_prepare.c').write_text(prefix,encoding='utf-8')
sources=['Shared/ring_buffer.c','Shared/dma_cursor.c','Shared/router.c','Shared/fw_header.c','Shared/crc16.c','Shared/crc32.c','Shared/image_contract.c','Shared/text_reply.c',
 'Bootloader/User/BSP/uart_ll.c','Bootloader/User/BSP/spi_bus.c','Bootloader/User/BSP/backup_domain.c','Bootloader/User/BSP/internal_flash.c',
 'Bootloader/User/Drivers/W25Q64/w25q64.c','Bootloader/User/Drivers/W25Q64/w25q64_storage.c',
 'Bootloader/User/IAP/store.c','Bootloader/User/IAP/image_store.c','Bootloader/User/IAP/metadata.c','Bootloader/User/IAP/iap_fsm.c','Bootloader/User/IAP/upgrade_log.c',
 'Bootloader/User/IAP/product_boot.c','Bootloader/User/Protocol/ymodem.c','Bootloader/User/Protocol/boot_protocol.c','Tests/host/uart_ll_connected_tests.c']
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DUART_HOST_TEST','-DUART_LL_HOST_TEST','-DSPI_BUS_HOST_TEST','-DIAP_STORAGE_ENABLED','-DIAP_PRODUCT_ENABLED','-DIAP_PHASE8_ENABLED','-DIAP_PROVISION_TEST_ONLY']
args+=['-I'+str(p) for p in [O]+[R/p for p in ['Tests/host/ll_stubs','Tests/host/stubs','Shared','Bootloader/User/BSP','Bootloader/User/IAP','Bootloader/User/Protocol','Bootloader/User/Drivers/W25Q64']]]
commands=[];objects=[]
for i,source in enumerate([R/s for s in sources]+[O/'jump_prepare.c']):
    obj=O/(str(i)+'.o');objects.append(str(obj));cmd=args+(['-DBootProtocol_Poll=Connected_Protocol_Poll'] if source.name=='boot_protocol.c' else [])+['-c',str(source),'-o',str(obj)]
    commands.append(cmd);p=subprocess.run(cmd,capture_output=True,text=True)
    (O/(str(i)+'_build.log')).write_text(p.stdout+p.stderr);p.check_returncode()
cmd=['gcc']+objects+['-o',str(O/'host.exe')];commands.append(cmd)
(O/'commands.json').write_text(json.dumps(commands,indent=2))
p=subprocess.run(cmd,capture_output=True,text=True);(O/'link.log').write_text(p.stdout+p.stderr);p.check_returncode()
results=[]
for mode in range(4):
    p=subprocess.run([str(O/'host.exe'),str(mode)],capture_output=True,text=True,timeout=60)
    (O/f'mode_{mode}.log').write_text(p.stdout+p.stderr)
    results.append(dict(mode=mode,returncode=p.returncode,output=p.stdout,stderr=p.stderr))
report=dict(scope='Actual connected product modules; SPI NOR/Flash/USART DMA register models; Jump Commit/Recovery electrical sampling/LSI/IWDG hardware are substitutes',results=results,source_sha256={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources},hardware='NOT RUN')
(O/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(results));sys.exit(any(x['returncode'] for x in results))

"""真实owner C主机测试；新目录保存全部输出，无芯片操作。"""
from pathlib import Path
import json, subprocess, sys
R=Path(__file__).resolve().parents[1]
O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DIAP_STORAGE_ENABLED']
args+=['-I'+str(R/p) for p in ['Shared','Bootloader/User/BSP','Bootloader/User/IAP','Bootloader/User/Drivers/W25Q64']]
sources=['Bootloader/User/BSP/internal_flash.c','Shared/image_contract.c',
         'Shared/crc32.c','Tests/host/internal_flash_tests.c']
if len(sys.argv)>2 and sys.argv[2]=='6':
    sources=['Bootloader/User/BSP/internal_flash.c','Shared/image_contract.c','Shared/crc32.c','Shared/fw_header.c',
             'Bootloader/User/Drivers/W25Q64/w25q64.c','Bootloader/User/Drivers/W25Q64/w25q64_storage.c',
             'Bootloader/User/IAP/store.c','Bootloader/User/IAP/image_store.c','Tests/host/storage_tests.c']
if len(sys.argv)>2 and sys.argv[2] in ('7','8fsm'):
    args+=['-DIAP_PROVISION_TEST_ONLY']
    sources=['Bootloader/User/BSP/internal_flash.c','Shared/image_contract.c','Shared/crc32.c','Shared/fw_header.c',
             'Bootloader/User/IAP/image_store.c','Bootloader/User/IAP/metadata.c','Bootloader/User/IAP/iap_fsm.c',
             'Tests/host/transaction_tests.c']
    if sys.argv[2]=='8fsm':
        args+=['-DIAP_PHASE8_ENABLED'];sources+=['Bootloader/User/IAP/upgrade_log.c']
if len(sys.argv)>2 and sys.argv[2]=='8':
    sources=['Shared/watchdog_contract.c','Shared/text_reply.c','Shared/fw_header.c','Shared/crc32.c',
             'Shared/image_contract.c','Bootloader/User/IAP/upgrade_log.c','Tests/host/phase8_tests.c']
if len(sys.argv)>3:
    assert sys.argv[2] in ('7','8fsm')
    sources=[sys.argv[3] if s=='Bootloader/User/IAP/iap_fsm.c' else s for s in sources]
exe=O/'host.exe'
p=subprocess.run(args+[str(R/s) for s in sources]+['-o',str(exe)],capture_output=True,text=True)
(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
p=subprocess.run([str(exe)],capture_output=True,text=True)
(O/'run.log').write_text(p.stdout+p.stderr);p.check_returncode()
(O/'results.json').write_text(json.dumps(json.loads(p.stdout),indent=2));print(p.stdout)

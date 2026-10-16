from pathlib import Path
import sys,subprocess,json,hashlib,shutil
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
source=R/'Bootloader/User/IAP/product_boot.c';shutil.copy2(source,O/source.name)
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DIAP_STORAGE_ENABLED','-DIAP_PRODUCT_ENABLED','-DIAP_PHASE8_ENABLED']
includes=['Tests/host/fixture_stubs','Shared','Bootloader/User/IAP','Bootloader/User/BSP','Bootloader/User/Protocol','Bootloader/User/Drivers/W25Q64']
args+=['-I'+str(R/p) for p in includes]+['-include',str(R/'Tests/host/fixture_stubs/stm32f1xx_hal.h')]
args+=[str(source),str(R/'Shared/text_reply.c'),str(R/'Tests/host/product_flow_tests.c'),'-o',str(O/'host.exe')]
(O/'inputs.json').write_text(json.dumps(dict(command=args,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest()),indent=2))
p=subprocess.run(args,capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
results=[]
for mode in range(5):
    p=subprocess.run([str(O/'host.exe'),str(mode)],capture_output=True,text=True,timeout=10)
    (O/f'mode_{mode}.log').write_text(p.stdout+p.stderr);p.check_returncode();results.append(json.loads(p.stdout))
(O/'results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))

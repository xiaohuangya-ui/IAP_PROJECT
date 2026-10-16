"""真实C的O3主机验证入口，独立HAL mock；不导入serial。"""
from pathlib import Path
import subprocess,json,sys,hashlib
R=Path(__file__).resolve().parents[1];phase=int(sys.argv[1]);O=R/(sys.argv[2] if len(sys.argv)>2 else f'Evidence/phase234_20261013/phase{phase}')
O.mkdir(parents=True,exist_ok=True)
assert not (O/'host_build.log').exists(),'Refusing to overwrite evidence'
if phase==4:
    fixture=subprocess.run([sys.executable,str(R/'Tests/host/pack_tests.py'),str(O)],capture_output=True,text=True)
    (O/'fixture_generation.log').write_text(fixture.stdout+fixture.stderr);fixture.check_returncode()
sources={2:['Shared/ring_buffer.c','Shared/dma_cursor.c','Bootloader/User/BSP/uart_dma.c','Tests/host/dma_tests.c'],3:['Shared/router.c','Application/User/App/app_shell.c','Tests/host/router_tests.c'],4:['Shared/crc32.c','Shared/crc16.c','Shared/fw_header.c','Shared/image_contract.c','Bootloader/User/Protocol/ymodem.c','Shared/router.c','Tests/host/ymodem_tests.c']}[phase]
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DUART_HOST_TEST','-DPROTOCOL_HOST_TEST']
if len(sys.argv)>3 and sys.argv[3]=='storage':args+=['-DIAP_STORAGE_ENABLED']
args+=['-I'+str(R/p) for p in ['Tests/host/dma_stubs','Shared','Bootloader/User/BSP','Bootloader/User/Protocol','Application/User/App']]
args+=[str(R/p) for p in sources];exe=O/'host.exe'
p=subprocess.run(args+['-o',str(exe)],capture_output=True,text=True);(O/'host_build.log').write_text(p.stdout+p.stderr);p.check_returncode()
runargs=[str(exe)]
if phase==4:runargs += [str(O/'crc_oracle.bin'),str(O/'golden.fw')]
p=subprocess.run(runargs,capture_output=True,text=True);(O/'host_run.log').write_text(p.stdout+p.stderr);p.check_returncode()
report=json.loads(p.stdout);report['source_sha256']={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources}
report['executable_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest();report['scope']='Actual C at O3, isolated host; no hardware claim'
(O/'host_results.json').write_text(json.dumps(report,indent=2)+'\n');print(p.stdout)

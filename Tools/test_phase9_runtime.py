"""Run actual App tasks on deterministic Win32 fiber boundary model; never accesses COM."""
from pathlib import Path
import subprocess,sys,json,hashlib,os
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
sources=['Application/User/App/app_runtime.c','Application/User/App/app_model.c','Application/User/App/app_shell.c','Application/User/BSP/app_temperature.c','Application/User/BSP/app_oled_port.c','Application/User/BSP/watchdog_handoff.c','Application/User/Drivers/SSD1306/ssd1306.c','Shared/watchdog_contract.c','Shared/router.c','Tests/host/phase9_runtime_tests.c']
sources.append('Application/User/App/app_version.c')
history="--history" in sys.argv
if history:sources += ["Application/User/App/app_history.c","Shared/crc32.c","Shared/image_contract.c"]
rows=[]
for version in [1,2,3]:
 args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DPROTOCOL_HOST_TEST','-DROUTER_ROLE=2',f'-DAPP_RELEASE_VERSION={version}']
 if history:args+=['-DAPP_HISTORY_ENABLED']
 if version==3:args=[a for a in args if not a.startswith('-DAPP_RELEASE_VERSION')]+['-DAPP_RELEASE_VERSION=2','-DAPP_TEST_TARGET','-DAPP_HEALTH_FAIL_TEST']
 args+=['-I'+str(R/p) for p in ['Tests/host/phase9_stubs','Application/User/App','Application/User/BSP','Application/User/Drivers/SSD1306','Bootloader/User/BSP','Shared']]
 args+=[str(R/p) for p in sources]+['-o',str(O/f'v{version}.exe')]
 (O/f'v{version}_inputs.json').write_text(json.dumps(dict(command=args,sources={p:hashlib.sha256((R/p).read_bytes()).hexdigest() for p in sources}),indent=2))
 p=subprocess.run(args,capture_output=True);(O/f'v{version}_build.log').write_bytes(p.stdout+p.stderr);p.check_returncode()
 for mode in ([18] if version==3 else list(range(18))+[19]+([20,21,22,23,24] if history else [])):
  env=os.environ.copy();env['P9_FRAME_PATH']=str(O/f'v{version}_frame.bin')
  p=subprocess.run([str(O/f'v{version}.exe'),str(mode)],capture_output=True,timeout=20,env=env)
  (O/f'v{version}_mode{mode}.log').write_bytes(p.stdout+p.stderr);p.check_returncode();rows.append(json.loads(p.stdout));print(version,p.stdout.decode().strip(),flush=True)
(O/'results.json').write_text(json.dumps(rows,indent=2))

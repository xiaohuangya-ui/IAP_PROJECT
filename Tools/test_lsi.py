from pathlib import Path
import subprocess,sys,json
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DIAP_PHASE8_ENABLED']
cmd+=['-I'+str(R/p) for p in ['Tests/host/lsi_stubs','Bootloader/User/BSP','Application/User/BSP','Shared']]
cmd+=[str(R/p) for p in ['Tests/host/lsi_tests.c','Bootloader/User/BSP/lsi_measure.c','Bootloader/User/BSP/iwdg.c','Shared/watchdog_contract.c','Application/User/BSP/watchdog_handoff.c']]
exe=O/'host.exe';p=subprocess.run(cmd+['-o',str(exe)],capture_output=True,text=True)
(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode();results=[]
for mode in range(9):
    p=subprocess.run([str(exe),str(mode)],capture_output=True,text=True)
    (O/f'mode_{mode}.log').write_text(p.stdout+p.stderr);p.check_returncode();results.append(json.loads(p.stdout))
(O/'results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))

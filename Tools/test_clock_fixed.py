from pathlib import Path
import subprocess,sys,json,hashlib,shutil
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-I'+str(R/'Tests/host/clock_stubs'),'-I'+str(R/'Bootloader/User/BSP')]
cmd += [str(R/'Tests/host/clock_tests.c'),str(R/'Bootloader/User/BSP/boot_clock.c'),'-o',str(O/'host.exe')]
p=subprocess.run(cmd,capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
results=[]
for mode in range(15):
    p=subprocess.run([str(O/'host.exe'),str(mode)],capture_output=True,text=True,timeout=20)
    (O/f'mode_{mode}.log').write_text(p.stdout+p.stderr);p.check_returncode();results.append(json.loads(p.stdout))
shutil.copy2(R/'Bootloader/User/BSP/boot_clock.c',O/'boot_clock.c')
(O/'results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))

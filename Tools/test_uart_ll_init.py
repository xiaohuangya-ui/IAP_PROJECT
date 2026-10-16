from pathlib import Path
import sys,subprocess,json
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror']+['-I'+str(R/p) for p in ['Tests/host/ll_init_stubs','Tests/host/ll_stubs']]+[str(R/p) for p in ['Bootloader/User/BSP/uart_ll_init.c','Tests/host/uart_ll_init_tests.c']]+['-o',str(O/'host.exe')]
(O/'command.json').write_text(json.dumps(cmd,indent=2));p=subprocess.run(cmd,capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
p=subprocess.run([str(O/'host.exe')],capture_output=True,text=True);(O/'run.log').write_text(p.stdout+p.stderr);p.check_returncode();(O/'results.json').write_text(p.stdout);print(p.stdout)

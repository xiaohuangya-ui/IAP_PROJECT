from pathlib import Path
import sys,subprocess,json
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror']+['-I'+str(R/p) for p in ['Tests/host/stubs','Application/User/BSP','Shared']]+[str(R/p) for p in ['Application/User/BSP/backup_domain.c','Tests/host/phase9_bkp_task_tests.c']]+['-o',str(O/'test.exe')]
(O/'command.json').write_text(json.dumps(args,indent=2))
p=subprocess.run(args,capture_output=True);(O/'build.log').write_bytes(p.stdout+p.stderr);p.check_returncode()
p=subprocess.run([str(O/'test.exe')],capture_output=True);(O/'run.log').write_bytes(p.stdout+p.stderr);p.check_returncode();print(p.stdout.decode())

"""Actual portable App algorithms/driver, independent temperature oracle and negative ports."""
from pathlib import Path
import subprocess,sys,json,hashlib
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
sources=['Application/User/App/app_model.c','Application/User/Drivers/SSD1306/ssd1306.c','Tests/host/phase9_model_tests.c']
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror']+['-I'+str(R/p) for p in ['Application/User/App','Application/User/Drivers/SSD1306']]+[str(R/p) for p in sources]+['-o',str(O/'test.exe')]
(O/'inputs.json').write_text(json.dumps(dict(command=args,sources={p:hashlib.sha256((R/p).read_bytes()).hexdigest() for p in sources}),indent=2))
p=subprocess.run(args,capture_output=True);(O/'build.log').write_bytes(p.stdout+p.stderr);p.check_returncode()
p=subprocess.run([str(O/'test.exe')],capture_output=True);(O/'run.log').write_bytes(p.stdout+p.stderr);p.check_returncode();print(p.stdout.decode())

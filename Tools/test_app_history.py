"""Actual C persistent history and App-only SPI/range tests; no COM/device access."""
from pathlib import Path
import subprocess,sys,json,hashlib
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
assert Path(sys.executable).resolve()==(R/'.venv/Scripts/python.exe').resolve()
sources=['Application/User/App/app_history.c','Shared/crc32.c','Shared/image_contract.c','Tests/host/app_history_tests.c']
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-I'+str(R/'Shared'),'-I'+str(R/'Application/User/App')]+[str(R/p) for p in sources]+['-o',str(O/'history.exe')]
rows=[]
port_sources=['Application/User/BSP/app_history_port.c','Tests/host/app_history_port_tests.c']
port_cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DIAP_STORAGE_ENABLED']+['-I'+str(R/p) for p in ['Tests/host/history_stubs','Application/User/App','Application/User/BSP','Bootloader/User/BSP','Bootloader/User/Drivers/W25Q64']]+[str(R/p) for p in port_sources]+['-o',str(O/'port.exe')]
for command in [cmd,[str(O/'history.exe')],port_cmd,[str(O/'port.exe')]]:
    p=subprocess.run(command,capture_output=True,timeout=240)
    rows.append(dict(command=command,returncode=p.returncode,stdout=p.stdout.decode(errors='replace'),stderr=p.stderr.decode(errors='replace')))
    (O/'results.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');p.check_returncode()
(O/'inputs.json').write_text(json.dumps({s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources+port_sources},indent=2),encoding='utf-8')
print(rows[-1]['stdout'])

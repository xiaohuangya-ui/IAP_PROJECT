from pathlib import Path
import subprocess,json,hashlib,sys
R=Path(__file__).resolve().parents[1];O=R/(sys.argv[1] if len(sys.argv)>1 else 'Evidence/phase234_20261013/phase4/service_regression');O.mkdir(parents=True,exist_ok=True)
assert not (O/'results.json').exists(),'Do not overwrite sealed test'
sources=['Bootloader/User/Protocol/boot_protocol.c','Bootloader/User/Protocol/ymodem.c','Shared/router.c','Shared/fw_header.c','Shared/crc16.c','Shared/crc32.c','Shared/image_contract.c','Tests/host/service_tests.c']
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror']+['-I'+str(R/p) for p in ['Shared','Bootloader/User/Protocol','Bootloader/User/BSP']]+[str(R/p) for p in sources]
exe=O/'host.exe';p=subprocess.run(args+['-o',str(exe)],capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
cases=[]
for args in [[],['sink']]:
    p=subprocess.run([str(exe)]+args,capture_output=True,text=True);(O/('sink_run.log' if args else 'release_run.log')).write_text(p.stdout+p.stderr);p.check_returncode();cases.append(json.loads(p.stdout))
report=dict(result='PASS',assertions=sum(x['assertions'] for x in cases),cases=cases,source_sha256={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources},executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
(O/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['result','assertions']}))

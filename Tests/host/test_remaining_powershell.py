"""PowerShell5.1用户流程验证：所有Programmer调用替换为文件模拟，绝不连接SWD。"""
from pathlib import Path
import subprocess,sys,json,shutil,os
R=Path(__file__).resolve().parents[2]
O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False);(O/'build').mkdir()
for target in ['PHASE2_BOOT_TEST','PHASE3_APP_START_TEST','PHASE3_APP_TEST']:
    shutil.copyfile(R/'Evidence/phase234_20261013/remaining_offline_20261013/build'/(target+'.hex'),O/'build'/(target+'.hex'))
fake=O/'fake_programmer.py'
fake.write_text('''from pathlib import Path
import sys,json
sys.path.insert(0,TOOLS)
from audit_closure import hexdata
O=Path(__file__).parent;state=O/'state.bin';args=sys.argv[1:]
with (O/'calls.jsonl').open('a') as f:f.write(json.dumps(args)+'\\n')
assert args[:6]==['-c','port=SWD','mode=UR','reset=HWrst','freq=950',args[5]]
data=bytearray(state.read_bytes()) if state.exists() else bytearray((i*17+3)&255 for i in range(65536))
if '-u' in args:
 i=args.index('-u');assert args[i+1:i+3]==['0x08000000','65536'];Path(args[i+3]).write_bytes(data)
elif '-e' in args:
 pages=list(map(int,args[args.index('-e')+1:]));assert pages in [list(range(10)),list(range(10))+list(range(16,28))]
 for page in pages:data[page*1024:(page+1)*1024]=b'\\xff'*1024
elif '-d' in args:
 assert '--skipErase' in args and '-v' in args
 for address,value in hexdata(Path(args[args.index('-d')+1])).items():data[address-0x08000000]=value
else:raise AssertionError('Unexpected simulated CLI command')
state.write_bytes(data)
'''.replace('TOOLS',repr(str(R/'Tools'))),encoding='utf-8')
source=(R/'Tools/prepare_remaining_hardware.ps1').read_text(encoding='utf-8-sig')
source=source.replace('$projectRoot = Split-Path $PSScriptRoot -Parent',"$projectRoot = '"+str(R)+"'")
source=source.replace("$remainingRoot = Join-Path $projectRoot 'Evidence\\phase234_20261013\\remaining_offline_20261013'","$remainingRoot = '"+str(O)+"'")
source=source.replace('& $remainingCli @CliArgs',"& $remainingPython '"+str(fake)+"' @CliArgs")
assert '& $remainingCli' not in source
runner=O/'simulated_user_flow.ps1';runner.write_text(source,encoding='utf-8-sig')
ps=r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
environment=dict(os.environ)
# 从PowerShell7启动5.1时补齐5.1原生模块搜索路径；不改变用户系统环境。
environment['PSModulePath']=r'C:\Windows\System32\WindowsPowerShell\v1.0\Modules'+';'+environment.get('PSModulePath','')
cases=[]
for unit in ['dma','app','app-fixed']:
    before=(O/'state.bin').read_bytes() if (O/'state.bin').exists() else bytes((i*17+3)&255 for i in range(65536))
    p=subprocess.run([ps,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(runner),'-Unit',unit],capture_output=True,env=environment)
    (O/(unit+'_run.log')).write_bytes(p.stdout+p.stderr);p.check_returncode()
    directory=next(O.glob('readback_'+unit+'_*'))
    checks=json.loads((directory/'remaining_readback_checks.json').read_text());assert checks['result']=='PASS'
    p=subprocess.run([ps,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(runner),'-Unit','restore','-Baseline',str(directory)],capture_output=True,env=environment)
    (O/(unit+'_restore.log')).write_bytes(p.stdout+p.stderr);p.check_returncode()
    assert (O/'state.bin').read_bytes()==before
    cases.append(dict(unit=unit,download_verify_readback_restore='PASS',pages=checks['erased_pages']))
(O/'results.json').write_text(json.dumps(dict(result='PASS',scope='Windows PowerShell5.1, mock Programmer/file memory only, no hardware',cases=cases),indent=2))
print(json.dumps(cases))

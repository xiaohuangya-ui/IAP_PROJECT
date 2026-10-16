"""Test exact pre-assembly prefix of jump.c; Commit stays target-only, audited from AXF."""
from pathlib import Path
import subprocess
import json
import hashlib
import sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/(sys.argv[1] if len(sys.argv)>1 else 'Evidence/phase0')
OUT.mkdir(parents=True,exist_ok=True)
assert not (OUT/'jump_prepare_host_build.log').exists(),'Refusing to overwrite old evidence'
source=(ROOT/'Bootloader/User/IAP/jump.c').read_text(encoding='utf-8')
prefix=source.split('/* AC5内嵌汇编函数',1)[0]
assert 'int Jump_Prepare' in prefix and '__asm' not in prefix
fixture=OUT/'jump_prepare_host_prefix.c'
fixture.write_text(prefix,encoding='utf-8')
exe=OUT/'jump_prepare_host.exe'
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror']
args+=['-I'+str(ROOT/p) for p in ['Tests/host/stubs','Bootloader/User/IAP','Bootloader/User/BSP',
                                 'Bootloader/User/Drivers/W25Q64','Shared']]
args += [str(fixture),str(ROOT/'Shared/crc32.c'),str(ROOT/'Shared/image_contract.c'),
         str(ROOT/'Tests/host/jump_prepare_tests.c'),'-o',str(exe)]
build=subprocess.run(args,capture_output=True,text=True)
(OUT/'jump_prepare_host_build.log').write_text(build.stdout+build.stderr);build.check_returncode()
run=subprocess.run([str(exe)],capture_output=True,text=True)
(OUT/'jump_prepare_host_run.log').write_text(run.stdout+run.stderr);run.check_returncode()
report=json.loads(run.stdout)
report.update(scope='exact jump.c Prepare prefix, fixed host virtual address, isolated context/peripheral fixtures',
    prefix_sha256=hashlib.sha256(prefix.encode('utf-8')).hexdigest(),
    commit='not executed on host; real AC5 machine-code audit required',hardware='NOT RUN')
(OUT/'jump_prepare_host_results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

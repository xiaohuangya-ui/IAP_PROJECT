from pathlib import Path
import subprocess
import json
import sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/(sys.argv[1] if len(sys.argv)>1 else 'Evidence/phase0')
OUT.mkdir(parents=True,exist_ok=True)
assert not (OUT/'bsp_host_build.log').exists(),'Refusing to overwrite old evidence'
exe=OUT/'bsp_host.exe'
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror']
args += ['-I'+str(ROOT/p) for p in ['Tests/host/stubs','Bootloader/User/BSP','Bootloader/User/Drivers/W25Q64','Shared']]
args += [str(ROOT/p) for p in ['Bootloader/User/BSP/spi_bus.c','Bootloader/User/BSP/backup_domain.c',
                              'Bootloader/User/Drivers/W25Q64/w25q64.c','Tests/host/bsp_tests.c']]
build=subprocess.run(args+['-o',str(exe)],capture_output=True,text=True)
(OUT/'bsp_host_build.log').write_text(build.stdout+build.stderr);build.check_returncode()
run=subprocess.run([str(exe)],capture_output=True,text=True)
(OUT/'bsp_host_run.log').write_text(run.stdout+run.stderr);run.check_returncode()
report=json.loads(run.stdout)
report.update(scope='actual Boot SPI/W25/BKP C with isolated host registers/HAL transport',
              hardware='NOT RUN: timing/electrical/DBP synchronization/reset retention')
(OUT/'bsp_host_results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
app_exe=OUT/'app_bkp_host.exe'
build=subprocess.run(['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror',
    '-I'+str(ROOT/'Tests/host/stubs'),'-I'+str(ROOT/'Application/User/BSP'),'-I'+str(ROOT/'Shared'),
    str(ROOT/'Application/User/BSP/backup_domain.c'),str(ROOT/'Tests/host/app_bkp_tests.c'),
    '-o',str(app_exe)],capture_output=True,text=True)
(OUT/'app_bkp_host_build.log').write_text(build.stdout+build.stderr);build.check_returncode()
run=subprocess.run([str(app_exe)],capture_output=True,text=True)
(OUT/'app_bkp_host_run.log').write_text(run.stdout+run.stderr);run.check_returncode()
app_report=json.loads(run.stdout)
app_report.update(scope='actual App BKP C: DR1/2 requests, DR3 read-only, clock/DBP restore',hardware='NOT RUN')
(OUT/'app_bkp_host_results.json').write_text(json.dumps(app_report,indent=2)+'\n')
print(json.dumps(app_report,indent=2))

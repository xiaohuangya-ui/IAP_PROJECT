from pathlib import Path
import subprocess,json,hashlib
import argparse
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description='Actual UART/Ring C tests; preserve sealed evidence')
parser.add_argument('--evidence',required=True)
parser.add_argument('--uart-source',default='Bootloader/User/BSP/uart_port.c')
opts=parser.parse_args()
OUT=ROOT/opts.evidence
OUT.mkdir(exist_ok=True)
assert not (OUT/'host_results.json').exists(), 'Do not overwrite sealed evidence'
sources=['Shared/ring_buffer.c',opts.uart_source,'Tests/host/uart_tests.c']
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DUART_HOST_TEST']
args+=['-I'+str(ROOT/p) for p in ['Tests/host/uart_stubs','Shared','Bootloader/User/BSP']]
args+=[str(ROOT/p) for p in sources]
exe=OUT/'uart_host.exe'
build=subprocess.run(args+['-o',str(exe)],capture_output=True,text=True)
(OUT/'host_build.log').write_text(build.stdout+build.stderr);build.check_returncode()
run=subprocess.run([str(exe)],capture_output=True,text=True)
(OUT/'host_run.log').write_text(run.stdout+run.stderr);run.check_returncode()
report=json.loads(run.stdout)
sources+=['Tests/host/uart_stubs/usart.h','Bootloader/User/BSP/uart_port.h','Shared/ring_buffer.h']
report.update(scope='Actual Ring/UART C at O3; isolated HAL mock. No hardware claim.',source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources})
(OUT/'host_results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

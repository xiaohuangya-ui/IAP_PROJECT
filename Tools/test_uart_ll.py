"""Actual isolated LL owner, with F1 register side effects; no device/serial."""
from pathlib import Path
import sys,subprocess,json,hashlib
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
sources=['Shared/ring_buffer.c','Shared/dma_cursor.c','Bootloader/User/BSP/uart_ll.c','Tests/host/uart_ll_tests.c']
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DUART_LL_HOST_TEST','-DUART_HOST_TEST','-DIAP_STORAGE_ENABLED']
cmd+=['-I'+str(R/p) for p in ['Tests/host/ll_stubs','Shared','Bootloader/User/BSP']]
cmd += [str(R/p) for p in sources]+['-o',str(O/'host.exe')]
(O/'command.json').write_text(json.dumps(cmd,indent=2))
p=subprocess.run(cmd,capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
p=subprocess.run([str(O/'host.exe')],capture_output=True,text=True,timeout=60);(O/'run.log').write_text(p.stdout+p.stderr);p.check_returncode()
report=json.loads(p.stdout);report.update(scope='Actual LL owner with independent F1 SR/DR, IFCR, DMA circular and EN fault model; not physical evidence',hardware='NOT RUN',source_sha256={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources})
(O/'results.json').write_text(json.dumps(report,indent=2));print(p.stdout)

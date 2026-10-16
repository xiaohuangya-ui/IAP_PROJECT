"""实际SPI owner测试；支持旧源负例，输出目录必须新建。"""
from pathlib import Path
import sys,subprocess,json,hashlib,shutil
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
source=R/(sys.argv[2] if len(sys.argv)>2 else 'Bootloader/User/BSP/spi_bus.c')
shutil.copy2(source,O/'spi_bus.c')
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DIAP_STORAGE_ENABLED','-DSPI_BUS_HOST_TEST']
cmd+=['-I'+str(R/p) for p in ['Tests/host/stubs','Bootloader/User/BSP']]
cmd+=[str(source),str(R/'Tests/host/spi_fixed_tests.c'),'-o',str(O/'host.exe')]
(O/'inputs.json').write_text(json.dumps(dict(command=cmd,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest()),indent=2))
p=subprocess.run(cmd,capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
p=subprocess.run([str(O/'host.exe')],capture_output=True,text=True,timeout=20)
(O/'run.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr);p.check_returncode()

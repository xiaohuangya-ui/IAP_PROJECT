"""Run real Shared C as a native process; supports installed 32-bit GCC."""
from pathlib import Path
import json
import random
import struct
import subprocess
import zlib
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Evidence/phase0'
corpus = OUT / 'crc_vectors.bin'
rng = random.Random(0x5030)
with corpus.open('wb') as stream:
    for size in [0,1,8,9,255,256,1023,49151,49152] + [rng.randrange(0,2049) for _ in range(3000)]:
        data = rng.randbytes(size)
        stream.write(struct.pack('<II',size,zlib.crc32(data)))
        stream.write(data)
exe = OUT / 'image_host.exe'
build = subprocess.run(['gcc', '-O3', '-std=c99', '-Wall', '-Wextra', '-Werror',
                '-I' + str(ROOT / 'Shared'), str(ROOT / 'Shared/crc32.c'),
                str(ROOT / 'Shared/image_contract.c'), str(ROOT / 'Tests/host/image_tests.c'),
                '-o', str(exe)], capture_output=True, text=True)
(OUT / 'host_build.log').write_text(build.stdout + build.stderr)
build.check_returncode()
run = subprocess.run([str(exe),str(corpus)], capture_output=True, text=True)
(OUT / 'host_run.log').write_text(run.stdout + run.stderr)
run.check_returncode()
report = json.loads(run.stdout)
report.update(seed='0x5030', scope='actual Shared C: zlib CRC/chunking/identity/vectors/P1 decision', hardware='NOT RUN')
(OUT / 'host_results.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))

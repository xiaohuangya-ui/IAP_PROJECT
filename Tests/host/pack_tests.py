"""独立Python CRC oracle及打包负例；不以被测C生成期望值。"""
from pathlib import Path
import sys,struct,random,zlib,binascii,json
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'Tools'))
from fw_pack import pack
if len(sys.argv)!=2:raise SystemExit('Specify a new evidence directory; historical evidence is read-only')
O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=True)
assert not any((O/n).exists() for n in ('golden.fw','crc_oracle.bin','pack_results.json')),'Refusing to overwrite evidence'
r=random.Random(103);checks=0;image=bytearray(r.randbytes(64));struct.pack_into('<II',image,0,0x20005000,0x08004009)
good=pack(image,0x10000,0x08004000)
assert len(good)==96 and struct.unpack_from('<I',good,28)[0]==zlib.crc32(good[:28]);checks+=1
(O/'golden.fw').write_bytes(good)
for size in [8,9,49151,49152]:
    data=bytearray(r.randbytes(size));struct.pack_into('<II',data,0,0x20005000,0x08004001)
    assert len(pack(data,1,0x08004000))==size+32;checks+=1
for data,version,base in [(b'',1,0x08004000),(b'1234567',1,0x08004000),(bytes(49153),1,0x08004000),(image,0,0x08004000),(image,0x100000000,0x08004000),(image,1,0x08000000)]:
    try:pack(data,version,base)
    except ValueError:checks+=1
    else:raise AssertionError('invalid package accepted')
for sp,pc in [(0x20000000,0x08004009),(0x20005004,0x08004009),(0x20004FFC,0x08004009),(0x20005000,0x08004008),(0x20005000,0x08004041)]:
    bad=bytearray(image);struct.pack_into('<II',bad,0,sp,pc)
    try:pack(bad,1,0x08004000)
    except ValueError:checks+=1
    else:raise AssertionError('invalid vector accepted')
with (O/'crc_oracle.bin').open('wb') as f:
    for i in range(1000):
        b=r.randbytes(r.randrange(1025));f.write(struct.pack('<HHI',len(b),binascii.crc_hqx(b,0),zlib.crc32(b))+b)
report=dict(result='PASS',assertions=checks,independent_crc_cases=1000,oracle='Python binascii.crc_hqx/zlib.crc32',scope='PC packer only; fromelf origin reviewed separately')
(O/'pack_results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))

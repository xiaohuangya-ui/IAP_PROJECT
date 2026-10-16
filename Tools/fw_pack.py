"""App fromelf .bin→32B Header+App；只打包，不串口/烧录/写存储。"""
from pathlib import Path
import argparse,struct,zlib,json,hashlib
APP_BASE=0x08004000
def pack(image:bytes,version:int,base:int)->bytes:
    if base!=APP_BASE: raise ValueError('App origin must be 0x08004000')
    if not 8<=len(image)<=49152: raise ValueError('Bootable App size must be 8..49152')
    if not 1<=version<=0xFFFFFFFF: raise ValueError('Nonzero uint32 version required')
    sp,pc=struct.unpack_from('<II',image)
    if not (0x20000000<sp<=0x20005000 and sp%8==0 and pc&1 and APP_BASE<=pc&~1<APP_BASE+len(image)):
        raise ValueError('Invalid MSP/Reset vector for this App binary')
    h=struct.pack('<7I',0x46574D47,1,version,len(image),zlib.crc32(image),base,0)
    return h+struct.pack('<I',zlib.crc32(h))+image
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('input',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--version',required=True,type=lambda s:int(s,0));p.add_argument('--base',required=True,type=lambda s:int(s,0),help='Explicit verified fromelf App origin')
    a=p.parse_args()
    if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():p.error('Refuse to overwrite existing artifact')
    image=a.input.read_bytes()
    try:result=pack(image,a.version,a.base)
    except ValueError as e:p.error(str(e))
    a.output.write_bytes(result)
    report=dict(scope='Packaging only; source App AXF/MAP origin must be audited separately',base=hex(a.base),version=hex(a.version),image_size=len(image),file_size=len(result),image_crc32=hex(zlib.crc32(image)),input_sha256=hashlib.sha256(image).hexdigest(),output_sha256=hashlib.sha256(result).hexdigest())
    a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()

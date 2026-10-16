"""Capture participating source/header identities before a real Rebuild."""
from pathlib import Path
import sys,json,hashlib,zipfile,xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[1];P=Path(sys.argv[1]).resolve();O=Path(sys.argv[2]).resolve()
assert not (O/'build_sources.zip').exists()
files={P};unit=P.parents[1]
units=[unit] if unit.name in ('Bootloader','Application') else [R/'Bootloader',R/'Application']
for node in E.parse(P).findall('.//LDads/ScatterFile'):
    if node.text:
        scatter=(P.parent/node.text.replace('\\','/')).resolve()
        if scatter.is_file():files.add(scatter)
for f in E.parse(P).findall('.//Groups/Group/Files/File'):
    files.add((P.parent/f.findtext('FilePath').replace('\\','/')).resolve())
for folder in [u/part for u in units for part in ('Core/Inc','User','Drivers','Middlewares')]+[R/'Shared']:
    if folder.is_dir():files.update(p.resolve() for p in folder.rglob('*.h'))
# Grouped user builds include original .c owners. Capture those exact inputs,
# recursively, rather than treating the wrapper alone as its source identity.
import re
include_dirs=[]
for node in E.parse(P).findall('.//IncludePath'):
    for name in (node.text or '').split(';'):
        if name:
            folder=(P.parent/name.replace('\\','/')).resolve()
            if folder.is_dir() and folder.is_relative_to(R):include_dirs.append(folder)
pending=list(files)
while pending:
    p=pending.pop()
    if p.suffix.lower() not in ('.c','.h'):continue
    # 厂商文件可能含非UTF-8版权注释；只按原始字节扫描ASCII include，
    # 哈希/归档仍保留每个文件的全部原字节，不转码第三方文件。
    for raw_name in re.findall(rb'^\s*#\s*include\s*"([^"\r\n]+\.(?:c|h))"',p.read_bytes(),re.M):
        name=raw_name.decode('utf-8')
        included=(p.parent/name).resolve();included.relative_to(R)
        if not included.is_file():
            # App可编译唯一Boot目录下的设备实现；其跨owner头必须按真实
            # IncludePath查找，不能假定App源码树已包含所有依赖。
            matches=[folder/name for folder in include_dirs if (folder/name).is_file()]
            if not matches:
                assert included.suffix.lower()=='.h',str(included)
                continue
            included=matches[0].resolve();included.relative_to(R)
        if included not in files:files.add(included);pending.append(included)
report={str(p.relative_to(R)).replace('\\','/'):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in sorted(files)}
with zipfile.ZipFile(O/'build_sources.zip','w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(files):z.write(p,str(p.relative_to(R)).replace('\\','/'))
(O/'build_sources.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

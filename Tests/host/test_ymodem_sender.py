"""实际Python Sender对实际C Protocol/Router/YModem的进程互操作回归。
不导入serial或访问硬件；UART/sink/clock仅PC mock，不证明DMA和持久存储。
"""
from pathlib import Path
import subprocess,sys,json,hashlib,importlib.util,binascii,struct,zlib
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/(sys.argv[1] if len(sys.argv)>1 else 'Evidence/phase234_20261013/extra_soh_tooling')
OUT.mkdir(parents=True,exist_ok=True)
assert not (OUT/'host_results.json').exists(),'Do not overwrite sealed test'
# 当前固件可合法修复；检查的是本次测试前后不变，而不是强制等于历史镜像。
# 历史manifest/镜像保留，用独立的新构建审查关联修复后的ARM产物。
snapshot=json.loads((ROOT/'Evidence/phase234_20261013/phase4/artifact_checks.json').read_text())
firmware={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in snapshot['source_sha256'] if p.startswith(('Bootloader/','Application/','Shared/'))}
spec=importlib.util.spec_from_file_location('sender',ROOT/'Tools/phase234_function_test.py')
sender=importlib.util.module_from_spec(spec);spec.loader.exec_module(sender)
assert 'serial' not in sys.modules
sources=['Bootloader/User/Protocol/boot_protocol.c','Bootloader/User/Protocol/ymodem.c','Shared/router.c','Shared/fw_header.c','Shared/crc16.c','Shared/crc32.c','Shared/image_contract.c','Tests/host/ymodem_sender_bridge.c']
exe=OUT/'receiver_bridge.exe'
args=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DPHASE4_TEST_ONLY']
args+=['-I'+str(ROOT/p) for p in ['Shared','Bootloader/User/Protocol','Bootloader/User/BSP']]
args+=[str(ROOT/p) for p in sources]+['-o',str(exe)]
build=subprocess.run(args,capture_output=True,text=True)
(OUT/'host_build.log').write_text(build.stdout+build.stderr);build.check_returncode()
class Bridge:
    def __init__(self):
        self.process=subprocess.Popen([str(exe)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.buffer=bytearray();self.writes=[];self.events=[]
    def exchange(self,command):
        self.process.stdin.write(command+'\n');self.process.stdin.flush()
        response=self.process.stdout.readline()
        assert response!='','C receiver terminated unexpectedly'
        return response.strip()
    def write(self,data):
        self.writes.append(data);response=bytes.fromhex(self.exchange('W'+data.hex()))
        self.events.append(dict(operation='WRITE',tx_hex=data.hex(),rx_hex=response.hex()))
        self.buffer.extend(response)
    def advance(self):
        response=bytes.fromhex(self.exchange('T1000'))
        self.events.append(dict(operation='ADVANCE',virtual_ms=1000,rx_hex=response.hex()))
        self.buffer.extend(response)
    def read(self,n,timeout=3):
        for _ in range(int(timeout)+1):
            if len(self.buffer)>=n:break
            self.advance()
        assert len(self.buffer)>=n,'C receiver response timeout'
        response=bytes(self.buffer[:n]);del self.buffer[:n];return response
    def line(self,timeout=3):
        for _ in range(int(timeout)+1):
            if b'\r\n' in self.buffer:break
            self.advance()
        assert b'\r\n' in self.buffer,'C receiver CRLF timeout'
        n=self.buffer.index(b'\r\n')+2;response=bytes(self.buffer[:n]);del self.buffer[:n];return response
    def command(self,data,expected):
        self.write(data);response=self.line();assert expected in response;return response
    def stats(self):return json.loads(self.exchange('Q'))
    def close(self):
        self.process.stdin.close();stderr=self.process.stderr.read();code=self.process.wait(timeout=5)
        assert code==0,(code,stderr)
class PartialBridge(Bridge):
    def write(self,data):
        super().write(data)
        if len(data)==20 and data[0] in (1,2):
            stats=self.stats();assert stats['writes']==0 and stats['file_bytes']==0 and stats['finishes']==0
            self.partial_pending=True
    def read(self,n,timeout=3):
        reply=super().read(n,timeout)
        if getattr(self,'partial_pending',False) and reply==b'\x15':
            stats=self.stats();assert stats['writes']==0 and stats['file_bytes']==0 and stats['finishes']==0
            self.partial_pending=False;self.partial_discard_verified=True
        return reply

def run(name,blob,size,negative='none'):
    bridge=PartialBridge() if negative=='partial' else Bridge()
    try:
        bridge.command(b'@IAP:VER\r\n',b'ACTIVE=UNKNOWN')
        if size is None:sender.ym(bridge,blob,negative)
        else:sender.ym(bridge,blob,negative,size)
        rejected=negative in ('length','header','image','finalblock','cancel','timeout','startup','canceldata','retrylimit')+sender.HEADER_NEGATIVES+sender.VECTOR_NEGATIVES+sender.BLOCK0_REJECTIONS
        if rejected:assert bridge.line()==b'ERR YMODEM\r\n'
        bridge.write(b'\r\n');bridge.command(b'@IAP:VER\r\n',b'ACTIVE=UNKNOWN')
        stats=bridge.stats();stored=bytes.fromhex(stats.pop('storage_hex'))
        if not rejected:
            expected=(len(blob)+(size or 1024)-1)//(size or 1024)
            if negative=='mixed':
                remaining=len(blob);expected=0
                while remaining>0:
                    remaining-=size if expected%2==0 else 1152-size;expected+=1
            expected_blob=sender.mode_payload(blob) if negative=='mode' else blob
            assert stored==expected_blob and stats['writes']==expected and stats['finishes']==1 and stats['aborts']==0 and stats['begins']==1
            if negative=='mode':
                assert stored[32:40]==blob[32:40]
                for offset in sender.MODE_OFFSETS:assert stored[offset:offset+len(sender.MODE_COMMANDS)]==sender.MODE_COMMANDS
                data=bridge.writes[3:-5]
                assert len(data)==expected and all(p[0]==(1 if size==128 else 2) for p in data)
                replies=[x['rx_hex'] for x in bridge.events if x['operation']=='WRITE']
                assert replies[3:3+expected]==['06']*expected
        else:assert stats['finishes']==0 and stats['aborts']==(0 if negative in ('startup',)+sender.BLOCK0_REJECTIONS else 1)
        if negative in sender.BLOCK0_REJECTIONS:assert stats['begins']==0 and stats['writes']==0 and stored==b''
        if negative=='retrylimit':assert stats['begins']==1 and stats['writes']==0 and stored==b''
        if negative=='changedduplicate':
            first=[x for x in bridge.events if x['operation']=='WRITE' and x['tx_hex'].startswith(('0101fe','0201fe'))]
            assert len(first)==3 and [x['rx_hex'] for x in first]==['06','15','06']
            packets=[bytes.fromhex(x['tx_hex']) for x in first]
            assert packets[0]==packets[2] and packets[1]!=packets[0]
            assert binascii.crc_hqx(packets[1][3:-2],0)==int.from_bytes(packets[1][-2:],'big')
        if negative in ('mixed','padding0'):
            packets=[bytes.fromhex(x['tx_hex']) for x in bridge.events if x['operation']=='WRITE' and x['tx_hex'].startswith(('01','02'))][1:-1]
            wire=b''.join(p[3:-2] for p in packets)
            assert wire[:len(blob)]==blob
            assert wire[len(blob):]==bytes([0 if negative=='padding0' else 26])*(len(wire)-len(blob))
            if negative=='mixed':assert [len(p)-5 for p in packets]==[size if i%2==0 else 1152-size for i in range(expected)]
        if negative=='canceldata':
            assert stats['begins']==1 and stats['writes']==1 and stored==blob[:size] and len(stored)<len(blob)
        if negative=='partial':assert bridge.partial_discard_verified
        if name=='max_soh':
            data=bridge.writes[3:-5]
            assert len(data)==385 and [p[1] for p in data]==[(i+1)&255 for i in range(385)]
            assert [p[1] for p in data[254:257]]==[255,0,1]
            for p in data:
                assert p[0]==1 and len(p)==133 and p[1]^p[2]==255
                assert binascii.crc_hqx(p[3:-2],0)==int.from_bytes(p[-2:],'big')
            wire=b''.join(p[3:-2] for p in data)
            assert wire[:len(blob)]==blob and wire[len(blob):]==b'\x1a'*96
        (OUT/(name+'_events.json')).write_text(json.dumps(bridge.events,indent=2)+'\n')
        return dict(case=name,result='PASS',data_block_bytes=size or 1024,negative=negative,file_bytes=len(blob),receiver=stats)
    finally:bridge.close()
cases=[]
small=(ROOT/'Evidence/phase234_20261013/phase4/golden.fw').read_bytes()
maximum=(ROOT/'Evidence/phase234_20261013/phase4/max_transport.fw').read_bytes()
for name,blob,size in [('max_soh',maximum,128),('max_stx',maximum,1024),('default_stx',small,None),('small_soh',small,128)]:cases.append(run(name,blob,size))
for negative in ['crc','complement','sequence','duplicate','length','header','image','finalblock','cancel','timeout']:
    cases.append(run('soh_'+negative,small,128,negative))
mode_file=(ROOT/'Evidence/phase234_20261013/phase4/APP_RELEASE_transport.fw').read_bytes()
for size in [128,1024]:
    for negative in sender.EXTRA_CASES:cases.append(run(negative+'_'+str(size),mode_file,size,negative))
for size in [128,1024]:
    for negative in sender.HEADER_NEGATIVES+sender.VECTOR_NEGATIVES:
        invalid=sender.isolated_invalid_payload(mode_file,negative)
        if negative=='headercrc':assert zlib.crc32(invalid[:28])!=struct.unpack_from('<I',invalid,28)[0] and invalid[:28]==mode_file[:28]
        else:assert zlib.crc32(invalid[:28])==struct.unpack_from('<I',invalid,28)[0]
        assert zlib.crc32(invalid[32:])==struct.unpack_from('<I',invalid,16)[0]
        cases.append(run(negative+'_'+str(size),mode_file,size,negative))
for size in [128,1024]:cases.append(run('mode_'+str(size),mode_file,size,'mode'))
for size in [128,1024]:cases.append(run('partial_'+str(size),small,size,'partial'))
cases.append(run('startup_30s',small,128,'startup'))
for size in [128,1024]:
    for negative in ['canceldata','block0','eot','finalack']:
        cases.append(run(negative+'_'+str(size),mode_file,size,negative))
try:sender.mode_payload(small)
except AssertionError:cases.append(dict(case='mode_sample_too_short',result='PASS'))
else:raise AssertionError('Mode sample length guard missing')
# 模拟错误Router在有效YModem包内执行VER；额外ASCII回复必须使Sender失败。
class InjectAsciiReply(Bridge):
    def write(self,data):
        super().write(data)
        commands=[p+b'\r\n' for p in sender.MODE_COMMANDS.split(b'\r\n') if p]
        if data and data[0] in (1,2) and any(cmd in data for cmd in commands):
            self.injected=True
            self.buffer.extend(b'BOOT UART-IAP VER=1 ACTIVE=UNKNOWN\r\n')
b=InjectAsciiReply()
try:
    try:sender.ym(b,mode_file,'mode',128)
    except AssertionError:
        assert getattr(b,'injected',False),'Negative mock must really inject an ASCII reply'
        cases.append(dict(case='extra_ASCII_reply_rejected',result='PASS'))
    else:raise AssertionError('Sender accepted accidental ASCII execution')
finally:b.close()
# 负例：接收端真实NAK被传输mock改成ACK，Sender必须拒绝，不能输出假PASS。
class CorruptReply(Bridge):
    def read(self,n,timeout=3):
        data=super().read(n,timeout)
        return b'\x06' if data==b'\x15' else data
b=CorruptReply()
try:
    try:sender.ym(b,small,'crc',128)
    except AssertionError:cases.append(dict(case='unexpected_ACK_for_bad_CRC',result='PASS',scope='Sender rejects wrong peer response'))
    else:raise AssertionError('Sender accepted unexpected ACK')
finally:b.close()
try:sender.ym(None,small,'none',256)
except AssertionError:cases.append(dict(case='invalid_block_size',result='PASS'))
else:raise AssertionError('Invalid block size accepted')
assert 'serial' not in sys.modules
assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in firmware.items())
report=dict(result='PASS',cases=cases,count=len(cases),scope='Actual Python Sender and actual C BootProtocol/Router/YModem at O3; virtual UART/clock, HOST RAM sink only',firmware_source_inputs_unchanged=len(firmware),firmware_source_sha256=firmware,source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources+['Tools/phase234_function_test.py','Tests/host/test_ymodem_sender.py']},receiver_exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),hardware='NOT RUN; assistant did not open serial')
(OUT/'host_results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(result=report['result'],cases=report['count'],max_soh=cases[0],firmware_inputs_unchanged=len(firmware)),indent=2))

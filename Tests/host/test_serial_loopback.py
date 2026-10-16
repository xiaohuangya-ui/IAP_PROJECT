"""USB-TTL定位脚本的离线模拟；不打开COM、不写hardware证据。"""
import sys,threading,unittest,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools'))
from serial_loopback_test import probe,read_exact
class Wire:
    def __init__(self,drop_round=None,empty=False):
        self.ready=threading.Event();self.pending=b'';self.sent=[]
        self.drop_round=drop_round;self.empty=empty
    def write(self,data):
        i=len(self.sent);self.sent.append(data)
        self.pending=b'' if self.empty else data[:58]+data[59:] if i==self.drop_round else data
        self.ready.set();return len(data)
    def flush(self): pass
    def reader(self,n,t):
        if not self.ready.wait(1): raise AssertionError('write missing')
        self.ready.clear();return self.pending
class Tests(unittest.TestCase):
    def test_full_100_rounds(self):
        p=Wire();capture=bytearray();r,a,b=probe(p,100,capture,p.reader)
        self.assertEqual(r['result'],'PASS');self.assertEqual(r['completed_rounds'],100)
        self.assertEqual(bytes(capture),b''.join(p.sent));self.assertEqual(r['bytes_compared'],sum(map(len,p.sent)))
        self.assertEqual(hashlib.sha256(p.sent[7]).hexdigest(),'a74d324f671684c7252128cf12ae03a5ccf0f777a895ebbae5428faff280f4cd')
        self.assertEqual(r['payload_sha256'],hashlib.sha256(capture).hexdigest())
        self.assertIsNone(a);self.assertIsNone(b)
    def test_same_missing_byte_fails(self):
        p=Wire(drop_round=7);capture=bytearray();r,a,b=probe(p,100,capture,p.reader)
        self.assertEqual(r['result'],'FAIL');self.assertEqual(r['failed_round'],7)
        self.assertEqual(r['completed_rounds'],7);self.assertEqual(r['bytes_compared'],898)
        self.assertEqual(len(a),511);self.assertEqual(len(b),510)
        self.assertEqual(a[:58]+a[59:],b);self.assertEqual(len(p.sent),8)
    def test_no_echo_fails(self):
        p=Wire(empty=True);r,a,b=probe(p,100,bytearray(),p.reader)
        self.assertEqual(r['result'],'FAIL');self.assertEqual(r['completed_rounds'],0)
        self.assertEqual(b,b'');self.assertEqual(len(a),1)
    def test_fragmented_binary_reads(self):
        parts=iter([b'\x00',b'\x07\x11',b'\x13\xff'])
        class P:
            def read(self,n): return next(parts)
        self.assertEqual(read_exact(P(),5,1),b'\x00\x07\x11\x13\xff')
    def test_deadline_keeps_short_read(self):
        ticks=iter([0,0.1,2])
        class P:
            def read(self,n): return b'a'
        self.assertEqual(read_exact(P(),3,1,lambda:next(ticks)),b'a')
if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Tests))
    out=ROOT/'Evidence/phase1/loopback_tooling_20261012';out.mkdir(exist_ok=True)
    sources=['Tools/serial_loopback_test.py','Tools/serial_io_diagnostics.py','Tests/host/test_serial_loopback.py']
    report=dict(result='PASS' if result.wasSuccessful() else 'FAIL',tests=result.testsRun,scope='Mocked host IO; NO COM/adapter/MCU operation',source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources})
    (out/'host_results.json').write_text(json.dumps(report,indent=2)+'\n')
    sys.exit(0 if result.wasSuccessful() else 1)

"""离线主机IO行为负例：不导入Serial、不打开COM、不生成hardware目录。"""
from pathlib import Path
import sys,unittest,threading,ctypes,types,json,hashlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools'))
from serial_io_diagnostics import exchange,WindowsErrorMonitor,windows_port_state
class Port:
    def __init__(self,payload,concurrent=False,short=False):
        self.payload=payload;self.concurrent=concurrent;self.short=short
        self.read_started=threading.Event();self.flushed=False;self.written=None
    def write(self,data):
        if self.concurrent:
            if not self.read_started.wait(1): raise AssertionError('reader did not run concurrently')
        else: assert not self.read_started.is_set()
        self.written=data
        return len(data)-1 if self.short else len(data)
    def flush(self): self.flushed=True
    def read(self,n,timeout):
        assert n==len(self.payload) and timeout==3
        self.read_started.set();return self.payload
class Tests(unittest.TestCase):
    def test_single_full_binary_write(self):
        data=bytes(range(256))*2
        for concurrent in [False,True]:
            p=Port(data,concurrent)
            self.assertEqual(exchange(p,data,p.read,3,concurrent),data)
            self.assertEqual(p.written,data);self.assertTrue(p.flushed)
    def test_short_receive_preserved(self):
        data=b'\x00\x07\x11\x13\xff';p=Port(data,True)
        def read(n,t): p.read_started.set();return data[:2]+data[3:]
        self.assertEqual(exchange(p,data,read,3,True),data[:2]+data[3:])
    def test_short_write_rejected(self):
        for concurrent in [False,True]:
            p=Port(b'abc',concurrent,True)
            with self.assertRaises(IOError): exchange(p,b'abc',p.read,3,concurrent)
            self.assertFalse(p.flushed)
    def test_reader_error_propagates(self):
        p=Port(b'abc',True)
        def read(n,t): p.read_started.set();raise RuntimeError('read failed')
        with self.assertRaisesRegex(RuntimeError,'read failed'): exchange(p,b'abc',read,3,True)
    def test_error_observer_retains_original_calls(self):
        state={'code':0,'ok':True,'calls':0}
        def original(handle,flags,status):
            state['calls']+=1
            flags._obj.value=state['code'];return state['ok']
        backend=types.SimpleNamespace(DWORD=ctypes.c_uint32,ClearCommError=original)
        m=WindowsErrorMonitor();m.install(backend)
        try:
            f=ctypes.c_uint32()
            self.assertTrue(backend.ClearCommError(None,ctypes.byref(f),None))
            state['code']=10
            self.assertTrue(backend.ClearCommError(None,ctypes.byref(f),None));self.assertEqual(f.value,10)
            state['ok']=False
            self.assertFalse(backend.ClearCommError(None,ctypes.byref(f),None))
            r=m.report();self.assertEqual(r['observed_calls'],3);self.assertEqual(r['api_failures'],1)
            self.assertEqual([e['flags'] for e in r['error_events']],[10]);self.assertEqual(state['calls'],3)
        finally: m.restore()
        self.assertIs(backend.ClearCommError,original)
    def test_no_monitor_claim(self):
        self.assertFalse(WindowsErrorMonitor().report()['available'])
    def test_dcb_failure_not_configured(self):
        backend=types.SimpleNamespace(DCB=ctypes.c_uint32,GetCommState=lambda *args:False)
        self.assertFalse(windows_port_state(types.SimpleNamespace(_port_handle=None),backend)['available'])
if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(Tests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    out=ROOT/'Evidence/phase1/host_io_review_20261012';out.mkdir(exist_ok=True)
    sources=['Tools/serial_io_diagnostics.py','Tools/phase1_serial_test.py','Tests/host/test_serial_io_diagnostics.py']
    report=dict(result='PASS' if result.wasSuccessful() else 'FAIL',tests=result.testsRun,scope='Mocked host IO only; NO COM/MCU execution',source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources})
    (out/'host_results.json').write_text(json.dumps(report,indent=2)+'\n')
    sys.exit(0 if result.wasSuccessful() else 1)

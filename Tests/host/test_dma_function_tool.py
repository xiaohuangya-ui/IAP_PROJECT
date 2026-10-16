"""BREAK功能工具负例：无串口、无硬件，证明不把未触发错误冒充恢复PASS。"""
from pathlib import Path
import importlib.util,json,sys
from unittest.mock import patch
R=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('dma_tool',R/'Tools/phase234_function_test.py')
tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
assert 'serial' not in sys.modules
before={'ERR':'0','BITS':'0','RECOVER':'0'}
class Fake:
    def __init__(self,bits,errors,recover,tc=True):
        self.events=[];self.writes=[];self.breaks=[];self.bits=bits;self.errors=errors;self.recover=recover;self.tc=tc
    def send_break(self,duration):self.breaks.append(duration)
    def command(self,b,expect):
        assert b==b's' and expect==b'P2 RX='
        return f'P2 RX=2 DROP=0 ERR={self.errors} RECOVER={self.recover} QUEUE=0 BITS={self.bits}\r\n'
    def write(self,b):self.writes.append(b)
    def read(self,n):assert n==256;return bytes(range(256)) if self.tc else bytes(256)
    def line(self):return b'P2 TC OK\r\n'
results=[]
with patch.object(tool.time,'sleep',lambda n:None):
    for name,bits,errors,recover,expected in [('not_triggered',0,0,0,'NOT_COVERED'),('real_error_recovered',4,1,1,'PASS'),('stale_bits_no_new_error',4,0,0,'NOT_COVERED')]:
        f=Fake(bits,errors,recover);report={'result':'FAIL'};tool.dma_break(f,f,before,report)
        actual='PASS' if report.get('error_recovery')=='PASS' else report['result']
        assert actual==expected and report['same_boot_tx_tc']=='PASS' and f.writes==[b't'] and f.breaks==[.02]
        results.append({'case':name,'result':'PASS','outcome':actual})
    for name,f in [('observed_error_not_recovered',Fake(4,1,0)),('tc_mismatch',Fake(0,0,0,False))]:
        try:tool.dma_break(f,f,before,{'result':'FAIL'})
        except AssertionError:results.append({'case':name,'result':'PASS','outcome':'reject'})
        else:raise AssertionError(name+' accepted')
out=R/sys.argv[1];out.mkdir(parents=True,exist_ok=True)
assert not (out/'results.json').exists()
report={'result':'PASS','cases':results,'scope':'Host mocks only; no serial imported'}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))

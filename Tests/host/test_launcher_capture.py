"""启动横幅采集真实Python路径，无serial导入或芯片访问。"""
from pathlib import Path
import json, sys
from unittest.mock import patch
R=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(R/'Tools'))
import remaining_function_test as tool

class Clock:
    now=0.
    def monotonic(self): return self.now
    def wall(self): return 1791889282+self.now

class Port:
    timeout=.1
    def __init__(self,c,queue): self.c=c; self.queue=list(queue)
    def read(self,n):
        if self.queue and self.queue[0][0]<=self.c.now+self.timeout:
            when,data=self.queue[0]; self.c.now=max(self.c.now,when)
            chunk=data[:n]
            if len(data)<=n: self.queue.pop(0)
            else: self.queue[0]=(when,data[n:])
            return chunk
        self.c.now+=self.timeout
        return b''
    def write(self,b): raise AssertionError('launcher must never transmit')
    def reset_input_buffer(self): raise AssertionError('must preserve input')

def run(out):
    out.mkdir(parents=True,exist_ok=False)
    a=b'P3 APP START TEST ONLY\r\n'; b=b'P3 APP START JUMP\r\n'
    cases=[('good',[(0,a+b)],True),
           ('190119',[(0,b'2 DMA TEST e/s/t/h\r\n')],False),
           ('wrong_then_valid',[(0,b'WRONG\r\n'),(.2,a+b)],False),
           ('second_wrong',[(0,a+b'WRONG\r\n')],False),
           ('partial_late',[(0,a[:-2]),(3.2,a[-2:]+b)],False),
           ('silent',[],False),('oversized',[(0,b'x'*1024)],False)]
    rows=[]
    for name,queue,success in cases:
        c=Clock(); events=[]; report={'result':'FAIL'}
        session=tool.CaptureSession(Port(c,queue),events)
        with patch.object(tool.time,'monotonic',c.monotonic),patch.object(tool.time,'time',c.wall):
            try: tool.app_launcher(session,report)
            except AssertionError as exc:
                assert not success
                report.update(error=str(exc),failed_stage=report['stage'])
            else: assert success
        if not success:
            assert report['app_commands']=='NOT RUN'
            failures=[e for e in events if e['direction']=='FIRST_FAIL']
            tails=[e for e in events if e['direction']=='FAIL_RESIDUAL_SUMMARY']
            assert len(failures)==len(tails)==1
            assert 1.999<=tails[0]['timing']['end_monotonic']-tails[0]['timing']['start_monotonic']<=2.001
            if name=='wrong_then_valid': assert tails[0]['hex']==(a+b).hex()
        assert not any(e['direction'].startswith('TX') for e in events)
        (out/(name+'.json')).write_text(json.dumps(dict(report=report,events=events),indent=2))
        rows.append(dict(case=name,result='PASS',expected_acceptance=success))
    assert 'serial' not in sys.modules
    (out/'results.json').write_text(json.dumps(rows,indent=2))
    print(json.dumps(rows))

if __name__=='__main__': run(R/sys.argv[1])

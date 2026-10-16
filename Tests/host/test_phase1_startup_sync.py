"""实际脚本启动同步函数的隔离测试；不导入/打开serial，不访问COM。"""
from pathlib import Path
import sys,json,hashlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools'))
from phase1_serial_test import synchronize_startup
from audit_closure import hexdata

banner=b'P1 UART TEST ONLY 115200 8N1; s=stats\r\n'
cases=[]
for name,extra,expected_error in [
    ('new_complete',b'P1 RESET CSR=04000003\r\n',None),
    ('new_multi_flag',b'P1 RESET CSR=1C000003\r\n',None),
    ('new_bad_csr',b'P1 RESET CSR=ZZZZZZZZ\r\n',RuntimeError),
    ('new_repeated_banner',banner,RuntimeError),
    ('new_missing_second_line',TimeoutError('missing CSR'),TimeoutError),
]:
    inputs=iter([banner,extra]); trace=[]
    def reader(marker,timeout):
        assert marker==b'\r\n'
        item=next(inputs);trace.append(('read',timeout,item))
        if isinstance(item,Exception):raise item
        return item
    try:
        result=synchronize_startup(reader,True)
        trace.append(('write',b's'))
        assert expected_error is None
        assert [entry[1] for entry in trace[:2]]==[30,5]
        assert result['startup_rcc_csr']==extra[13:21].decode()
        assert trace[-2][2]==extra
    except (RuntimeError,TimeoutError) as error:
        assert expected_error and isinstance(error,expected_error)
        assert not any(entry[0]=='write' for entry in trace)
    cases.append({'name':name,'result':'PASS'})

def old_reader(marker,timeout):
    assert marker==b'\r\n' and timeout==30
    return banner
assert synchronize_startup(old_reader,False)=={'phase1_startup_banner_observed':True}
cases.append({'name':'old_single_line','result':'PASS'})
try:
    synchronize_startup(lambda *_:b'P0 UART\r\n',False)
    raise AssertionError('Phase0 banner must be rejected')
except RuntimeError:pass
cases.append({'name':'wrong_phase_rejected','result':'PASS'})
for relative,expected in [
    ('Evidence/phase1/timing_review_20261013/build/PHASE1_BOOT_TEST.hex',True),
    ('Evidence/phase1/PHASE1_BOOT_TEST.hex',False),
]:
    data=hexdata(ROOT/relative)
    assert (b'P1 RESET CSR=' in bytes(data[a] for a in sorted(data)))==expected
cases.append({'name':'actual_old_new_hex_format_detection','result':'PASS'})
out=ROOT/'Evidence/phase1/startup_sync_fix_20261013'
out.mkdir(exist_ok=True)
assert not (out/'host_results.json').exists(),'Do not overwrite sealed evidence'
report={'result':'PASS','scope':'Actual startup synchronizer with isolated byte-reader; no hardware acceptance','cases':cases,'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['Tools/phase1_serial_test.py','Tests/host/test_phase1_startup_sync.py']}}
(out/'host_results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

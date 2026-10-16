from pathlib import Path
import subprocess,re,zlib,json,hashlib
import argparse
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description='Actual fixture C host checks; new evidence folder required')
parser.add_argument('--evidence',required=True)
args=parser.parse_args()
out=root/args.evidence
out.mkdir(exist_ok=True)
assert not (out/'fixture_host_results.json').exists(), 'Do not overwrite sealed host evidence'
sources=['Tests/hardware/phase1_uart.c','Tests/host/phase1_direction_tests.c','Shared/crc32.c']
exe=out/'fixture_host.exe'
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DPHASE1_TEST_ONLY']
cmd+=['-I'+str(root/p) for p in ['Tests/host/fixture_stubs','Shared','Bootloader/User/BSP']]
run=subprocess.run(cmd+[str(root/p) for p in sources]+['-o',str(exe)],capture_output=True,text=True)
(out/'fixture_host_build.log').write_text(run.stdout+run.stderr);run.check_returncode()

payload=bytes(i%256 for i in range(511));cases=[]
vector=(root/'Evidence/phase1/serial_assistant_20261012/loopback_511.bin').read_bytes()
for mode in range(10):
    wire=subprocess.check_output([str(exe),str(mode)])
    reset_line=b'P1 RESET CSR='+ (b'6C000003' if mode==9 else b'08000003')+b'\r\n'
    assert reset_line in wire  # preserve raw sticky flags before clearing, LSI low bits unchanged
    (out/f'host_case_{mode}.bin').write_bytes(wire)
    if mode>=7:
        fields={k.decode():int(v,16) for k,v in re.findall(rb'([A-Z0-9]+)=([0-9A-F]{8})',wire)}
        assert b'P1 PIN BEGIN 10s;' in wire and wire.endswith(b'\r\nP1 PIN END\r\n')
        assert fields['LOW']==(2000 if mode>=8 else 0), (mode,fields)
        # mock Service advances one millisecond before the first sample.
        assert fields['LOW']+fields['HIGH']==9999
        assert fields['ERR']==(4 if mode==8 else 0)
        assert fields['TOTAL']==fields['RECOVER']==(1 if mode==8 else 0)
        assert fields['RXDELTA']==0 and fields['CR1']==0x202C and fields['CR3']==1
        cases.append(dict(mode=mode,pin_probe=fields,result='PASS'));continue
    if mode==4:
        sent=wire.split(b'P1 TX511 BEGIN\r\n',1)[1].split(b'\r\nP1 TX511 END\r\n',1)[0]
        assert sent==vector and len(sent)==511
        cases.append(dict(mode=mode,transmitted_bytes=len(sent),result='PASS'));continue
    if mode<3:
        tail=wire.split(b'P1 DIAG READY\r\n',1)[1]
        echo,diag=tail.split(b'\r\nP1 DIAG RXDELTA=',1)
    else:
        head,diag=wire.split(b'\r\nP1 RX DIAG RXDELTA=',1)
        assert head.count(b'\r\n')==2  # startup banner + raw reset source; no RX acknowledgement
        echo=b''
    fields={k.decode():int(v,16) for k,v in re.findall(rb'([A-Z]+)=([0-9A-F]{8})',b'RXDELTA='+diag)}
    consumed=payload[:58]+payload[59:] if mode in (1,5) else payload
    expected_echo=payload[:58]+payload[59:] if mode in (1,2) else payload
    assert echo==(expected_echo if mode<3 else b'')
    assert fields['RXDELTA']==fields['FWD']==len(consumed),(mode,fields)
    assert fields['TXOK']==(len(consumed) if mode<3 else 0)
    assert fields['CRC']==zlib.crc32(consumed)
    assert fields['ERR']==fields['TOTAL']==fields['DROP']==fields['QUEUE']==0
    cases.append(dict(mode=mode,received_bytes=len(echo),diagnostic=fields,result='PASS'))
sources+=['Tests/hardware/phase1_tx_vector.h','Tests/host/fixture_stubs/stm32f1xx_hal.h']
report=dict(result='PASS',scope='Actual fixture C and CRC C with mocked UART; not MCU/USB evidence',cases=cases,source_sha256={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources})
(out/'fixture_host_results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

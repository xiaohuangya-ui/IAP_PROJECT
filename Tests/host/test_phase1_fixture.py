from pathlib import Path
import subprocess,re,zlib,json,hashlib
root=Path(__file__).resolve().parents[2]
out=root/'Evidence/phase1/diagnostic_20261012'
out.mkdir(exist_ok=True)
sources=['Tests/hardware/phase1_uart.c','Tests/host/phase1_fixture_tests.c','Shared/crc32.c']
exe=out/'fixture_host.exe'
cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DPHASE1_TEST_ONLY']
cmd+=['-I'+str(root/p) for p in ['Tests/host/fixture_stubs','Shared','Bootloader/User/BSP']]
run=subprocess.run(cmd+[str(root/p) for p in sources]+['-o',str(exe)],capture_output=True,text=True)
(out/'fixture_host_build.log').write_text(run.stdout+run.stderr);run.check_returncode()
payload=bytes(i%256 for i in range(511));cases=[]
for mode in range(3):
    wire=subprocess.check_output([str(exe),str(mode)])
    (out/f'host_case_{mode}.bin').write_bytes(wire)
    head,tail=wire.split(b'P1 DIAG READY\r\n',1)
    echo,diag=tail.split(b'\r\nP1 DIAG RXDELTA=',1)
    fields={k.decode():int(v,16) for k,v in re.findall(rb'([A-Z]+)=([0-9A-F]{8})',b'RXDELTA='+diag)}
    consumed=payload[:58]+payload[59:] if mode==1 else payload
    expected_echo=payload[:58]+payload[59:] if mode in (1,2) else payload
    assert echo==expected_echo,(mode,len(echo))
    assert fields['RXDELTA']==fields['FWD']==fields['TXOK']==len(consumed)
    assert fields['CRC']==zlib.crc32(consumed)
    assert fields['ERR']==fields['TOTAL']==fields['DROP']==fields['QUEUE']==0
    cases.append(dict(mode=mode,received_bytes=len(echo),diagnostic=fields,result='PASS'))
report=dict(result='PASS',scope='Actual fixture C and CRC C with mocked UART; not MCU/USB evidence',cases=cases,source_sha256={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources})
(out/'fixture_host_results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

from pathlib import Path
import importlib.util,tempfile,unittest,zlib,json,sys,subprocess
R=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('factory_client',R/'Tools/phase9_factory_client.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Port:
 def __init__(self,data=b'',lines=()):self.data=data;self.lines=list(lines);self.writes=[]
 def write(self,b):self.writes.append(b);return len(b)
 def read(self,n):b=self.data[:n];self.data=self.data[n:];return b
 def readline(self):return self.lines.pop(0)
class Tests(unittest.TestCase):
 def test_export(self):
  b=bytes(i%251 for i in range(0x26000));p=Port(b,[b'PHASE9 FACTORY TOOL\r\n',b'FACTORY DATA 155648\r\n',b'\r\n',f'FACTORY CRC32={zlib.crc32(b):08X}\r\n'.encode()])
  with tempfile.TemporaryDirectory(dir=R/'Evidence/phase9_20261015') as d:
   path=Path(d)/'backup.bin';got,crc=m.Client(p).backup(path);self.assertEqual(got,b);self.assertEqual(crc,zlib.crc32(b));self.assertEqual(json.loads(path.with_suffix('.json').read_text())['bytes'],len(b))
 def test_bad_crc(self):
  p=Port(bytes(0x26000),[b'FACTORY DATA 155648\r\n',b'FACTORY CRC32=00000000\r\n'])
  with tempfile.TemporaryDirectory(dir=R/'Evidence/phase9_20261015') as d:
   with self.assertRaises(AssertionError):m.Client(p).backup(Path(d)/'bad.bin')
 def test_refused(self):
  with self.assertRaises(RuntimeError):m.Client(Port(lines=[b'FACTORY REFUSED IDENTITY\r\n'])).provision(0)
 def test_stop_wait_restore(self):
  p=Port(lines=[b'FACTORY READY 8192\r\n']+[b'FACTORY ACK\r\n']*32+[b'FACTORY RESTORED META NO_JUMP\r\n']);b=bytes(i%251 for i in range(8192));m.Client(p).restore(b)
  self.assertEqual(b''.join(p.writes[1:]),b);self.assertEqual(len(p.writes),33)
 def test_no_auth_no_port(self):
  with tempfile.TemporaryDirectory(dir=R/'Evidence/phase9_20261015') as d:
   p=subprocess.run([sys.executable,str(R/'Tools/phase9_factory_client.py'),'--port','NEVER_OPEN','--action','provision-v1','--out',str(Path(d)/'no')],capture_output=True)
   self.assertEqual(p.returncode,2);self.assertFalse((Path(d)/'no').exists());self.assertIn(b'--authorize',p.stderr)
if __name__=='__main__':unittest.main()

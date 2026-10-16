from pathlib import Path
import importlib.util,unittest,struct,binascii
R=Path(__file__).resolve().parents[2];spec=importlib.util.spec_from_file_location('demo',R/'Tools/phase9_upgrade_demo.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Port:
 def __init__(self,nak=False,cancel=False):self.buffer=bytearray();self.writes=[];self.nak=nak;self.cancel=cancel;self.data=bytearray();self.eots=0;self.opened=False
 def write(self,b):
  self.writes.append(b)
  if b==b'@IAP:ENTER\r\n':self.buffer+=b'C'
  elif b==b'\x04':self.eots+=1;self.buffer+=b'\x15' if self.eots==1 else b'\x06C'
  else:
   assert b[0] in (1,2) and b[1]^b[2]==255 and struct.unpack('>H',b[-2:])[0]==binascii.crc_hqx(b[3:-2],0)
   if self.cancel:self.buffer+=b'\x18';return len(b)
   if b[1]==1 and self.nak:self.nak=False;self.buffer+=b'\x15';return len(b)
   if b[1]==0 and b[3]:self.opened=True;self.buffer+=b'\x06C'
   else:
    if b[1]:self.data+=b[3:-2]
    self.buffer+=b'\x06'
  return len(b)
 def read(self,n):b=bytes(self.buffer[:n]);del self.buffer[:n];return b
class Tests(unittest.TestCase):
 def test_package(self):
  p=R/'Evidence/phase9_20261015/delivery_final/release/APP_V2_RELEASE.fw';self.assertEqual(len(m.package(p)),21184)
 def test_known_crc(self):self.assertEqual(binascii.crc_hqx(b'123456789',0),0x31C3)
 def test_transfer(self):
  b=m.package(R/'Evidence/phase9_20261015/delivery_final/release/APP_V2_RELEASE.fw');p=Port();m.Link(p).ymodem(b);self.assertEqual(bytes(p.data[:len(b)]),b);self.assertEqual(p.eots,2);self.assertEqual(p.writes[-1],m.packet(0,bytes(128)))
 def test_nak_retry_same_packet(self):
  b=bytes(1100);p=Port(nak=True);m.Link(p).ymodem(b);data=[x for x in p.writes if len(x)==1029];self.assertEqual(data[0],data[1]);self.assertEqual(bytes(p.data[:len(b)]),b)
 def test_cancel_fail_closed(self):
  p=Port(cancel=True)
  with self.assertRaises(RuntimeError):m.Link(p).ymodem(bytes(40))
 def test_confirmed_status(self):
  class L:
   def command(self,c,e):return b'IAP STATE=1 DOWNLOAD=0 BACKUP=0 ATTEMPTS=0 ERROR=0 ACTIVE=131072 PENDING=131072 LOG_ERROR=0\r\n'
   def write(self,c):pass
   def line(self,t):return b'LOG END LOG_CORRUPT=0\r\n'
  self.assertEqual(m.confirmed_status(L())['fields']['STATE'],1)
 def test_pending_rejected(self):
  class L:
   def command(self,c,e):return b'IAP STATE=7 DOWNLOAD=1 BACKUP=1 ATTEMPTS=1 ERROR=0 ACTIVE=65536\r\n'
  with self.assertRaises(AssertionError):m.confirmed_status(L())
if __name__=='__main__':unittest.main()

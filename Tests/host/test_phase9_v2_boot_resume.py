from pathlib import Path
import sys,unittest,tempfile
from unittest.mock import patch
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'Tools'))
import phase9_v2_boot_resume as m
class Port:
 def __init__(self):self.buffer=bytearray();self.commands=[]
 def write(self,b):
  self.commands.append(b)
  replies={b'\r\n@APP:VERSION\r\n':b'ERR COMMAND\r\nAPP VER=2.0 MODE=NORMAL\r\n',b'@APP:TEMP\r\n':b'APP TJ_EST_X10=55 RAW=1879 STATUS=OK\r\n',b'@APP:UPDATE\r\n':b'APP UPDATE REQUESTED\r\nAPP ENTER BOOT\r\n',b'\r\n@IAP:VER\r\n':b'ERR COMMAND\r\nBOOT UART-IAP VER=1 ACTIVE=131072\r\n',b'@IAP:STATUS\r\n':b'IAP STATE=1 ACTIVE=131072 ATTEMPTS=0 DOWNLOAD=0 BACKUP=0 ERROR=0 LOG_ERROR=0\r\n',b'@IAP:LOG\r\n':b'LOG BEGIN\r\nLOG END LOG_CORRUPT=0\r\n'}
  self.buffer+=replies[b];return len(b)
 def read(self,n):out=bytes(self.buffer[:n]);del self.buffer[:n];return out
class Tests(unittest.TestCase):
 def test_offline_prior_preserves_failure(self):
  self.assertNotIn('serial',sys.modules)
  p=m.PRIOR/'result.json';before=p.read_bytes();audit=m.prior_audit()
  self.assertEqual(audit['prior_result_preserved'],'FAIL');self.assertEqual(p.read_bytes(),before)
 def test_forbids_download_and_binary(self):
  port=Port();link=m.QueryLink(port)
  for b in [b'@IAP:ENTER\r\n',b'\x04',bytes(133),b'@IAP:ERASE\r\n']:
   with self.assertRaises(RuntimeError):link.write(b)
  self.assertEqual(port.commands,[])
 def test_actual_query_flow_has_no_transfer(self):
  port=Port();link=m.QueryLink(port)
  with patch.object(m.m.time,'sleep'):reply=m.m.enter_and_query_boot(link)
  self.assertEqual(reply['fields']['STATE'],1)
  self.assertIn(b'@APP:UPDATE\r\n',port.commands);self.assertTrue(all(x in m.ALLOWED for x in port.commands))
 def test_bad_current_flash_blocks_serial(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);(root/'Evidence').mkdir()
   with patch.object(m,'ROOT',root),patch.object(m,'prior_audit',return_value={}),patch.object(m.m,'swd_read',return_value=bytes(65536)),patch.object(m,'serial_query') as serial,patch.object(sys,'argv',['resume','--execute']):
    with self.assertRaises(RuntimeError):m.main()
    serial.assert_not_called()
if __name__=='__main__':unittest.main()

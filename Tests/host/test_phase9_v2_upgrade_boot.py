"""Offline V2 tool guards; no COM/SWD access."""
from pathlib import Path
import sys,unittest,importlib.util,struct,tempfile,json,copy
from unittest.mock import patch
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'Tools'))
import phase9_v2_upgrade_boot as m
spec=importlib.util.spec_from_file_location('oldtests',R/'Tests/host/test_phase9_upgrade_demo.py');old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
BASE=(R/'Evidence/phase9_v1_oled_install_20261016_100359/after64k.bin').read_bytes()
NEW=(R/'Evidence/phase9_v2_tools_20261016/V2_FIXED_BOOT_FLASH64K_COMPARE_ONLY.bin').read_bytes()
def samples():
 a={'model':{'start_ms':4,'business_seq':4,'business_ms':2004,'uart_ms':2200,'adc_ok':2,'oled_ok':2,'trial':0,'fatal':0,'oled_ready':1,'uart_ready':1,'temp_ok':1},'trend':{'count':2,'next':2,'samples_x10':[100]*60},'stack_free_words':[152,152,100],'heap_min_bytes':2616}
 b=copy.deepcopy(a)
 for k in ['business_seq','business_ms','uart_ms','adc_ok','oled_ok']:b['model'][k]+=5
 b['trend']['count']=7;b['trend']['next']=7
 return a,b
class Tests(unittest.TestCase):
 def test_preflight_no_device(self):
  self.assertNotIn('serial',sys.modules);self.assertEqual(len(m.preflight()),21260)
 def test_v1_or_modified_package_rejected(self):
  b=bytearray(m.preflight());b[-1]^=1
  for data in [bytes(b),(m.RELEASE/'APP_V1_RELEASE.fw').read_bytes()]:
   with self.assertRaises(RuntimeError):m.package(data)
 def test_baseline_and_new_distinct(self):
  m.verify_flash(BASE);m.verify_flash(NEW,True)
  with self.assertRaises(RuntimeError):m.verify_flash(BASE,True)
  with self.assertRaises(RuntimeError):m.verify_flash(NEW)
 def test_roles_confirmed_versions(self):
  line=b'IAP STATE=1 ACTIVE=131072 ATTEMPTS=0 DOWNLOAD=0 BACKUP=0 ERROR=0 LOG_ERROR=0\r\n'
  self.assertEqual(m.confirmed(line)['ACTIVE'],131072)
  with self.assertRaises(RuntimeError):m.confirmed(line,65536)
  with self.assertRaises(RuntimeError):m.confirmed(line.replace(b'STATE=1',b'STATE=7'))
 def test_wire_name_bytes_and_nak(self):
  blob=m.preflight();port=old.Port(nak=True);m.RepairLink(port).ymodem(blob)
  self.assertEqual(port.writes[1][3:-2].split(b'\0',1)[0],b'APP_V2_RELEASE.fw')
  self.assertEqual(bytes(port.data[:len(blob)]),blob)
  packets=[p for p in port.writes if len(p)==1029];self.assertEqual(packets[0],packets[1])
 def test_cancel_and_invalid_blob_fail_closed(self):
  port=old.Port(cancel=True)
  with self.assertRaises(RuntimeError):m.RepairLink(port).ymodem(m.preflight())
  port=old.Port()
  with self.assertRaises(RuntimeError):m.RepairLink(port).ymodem(b'bad')
  self.assertEqual(port.writes,[])
 def test_trend_layout_and_bounds(self):
  ram=bytearray(20480);struct.pack_into('<60hBB',ram,0x27a8,*([100]*60),2,2)
  decoded=m.decode_ram(ram);self.assertEqual(decoded['trend']['count'],2)
  ram[0x27a8+120]=61
  with self.assertRaises(RuntimeError):m.decode_ram(ram)
 def test_progress_and_stalled_oled_rejected(self):
  a,b=samples();self.assertIn('PASS',m.runtime_progress(a,b))
  b['model']['oled_ok']=a['model']['oled_ok']
  with self.assertRaises(RuntimeError):m.runtime_progress(a,b)
 def test_no_trend_rejected(self):
  a,b=samples();b['trend']['count']=0
  with self.assertRaises(RuntimeError):m.runtime_progress(a,b)
 def test_baseline_mismatch_before_serial(self):
  blob=m.preflight()
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);(root/'Evidence').mkdir()
   with patch.object(m,'ROOT',root),patch.object(m,'preflight',return_value=blob),patch.object(m,'swd_read',return_value=bytes(65536)),patch.object(m,'with_serial') as serial,patch.object(sys,'argv',['tool','--execute']):
    with self.assertRaises(RuntimeError):m.main()
    serial.assert_not_called()
 def test_late_failure_never_leaves_pass_result(self):
  blob=m.preflight();a,b=samples()
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);(root/'Evidence').mkdir()
   def read(folder,name,addr,size):
    data=BASE if name=='before64k.bin' else NEW if size==65536 else bytes(20480)
    (folder/name).write_bytes(data);return data
   with patch.object(m,'ROOT',root),patch.object(m,'preflight',return_value=blob),patch.object(m,'swd_read',side_effect=read),patch.object(m,'with_serial',return_value={}),patch.object(m,'decode_ram',side_effect=[b,a,b]),patch.object(m,'runtime_progress',side_effect=RuntimeError('late failure')),patch.object(m.time,'sleep'),patch('builtins.input',side_effect=['OK','','']),patch.object(sys,'argv',['tool','--execute']):
    with self.assertRaises(RuntimeError):m.main()
   result=json.loads(next((root/'Evidence').glob('*/result.json')).read_text(encoding='utf-8'));self.assertEqual(result['result'],'FAIL')
 def test_normal_boot_wrong_version_rejected(self):
  class L:
   def command(self,*args):return b'APP VER=1.0 MODE=NORMAL\r\n'
  with self.assertRaises(RuntimeError):m.normal_app_query(L())
if __name__=='__main__':unittest.main()

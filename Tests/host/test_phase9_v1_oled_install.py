"""Offline guards and real packet sender; never opens serial or SWD."""
from pathlib import Path
import sys,unittest,importlib.util,struct,tempfile
from unittest.mock import patch
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'Tools'))
import phase9_install_v1_oled_fix as m
spec=importlib.util.spec_from_file_location('oldtests',R/'Tests/host/test_phase9_upgrade_demo.py')
old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)

class Tests(unittest.TestCase):
    def test_preflight_no_device(self):
        self.assertNotIn('serial',sys.modules);self.assertEqual(len(m.preflight()),20992)
    def test_modified_or_v2_rejected(self):
        b=bytearray(m.preflight());b[-1]^=1
        for data in [bytes(b),(m.RELEASE/'APP_V2_RELEASE.fw').read_bytes(),b'']:
            with self.assertRaises(RuntimeError):m.package(data)
    def test_full_flash_old_and_new_distinguished(self):
        before=(R/'Evidence/phase9_boot_fix_hw_20261016_091007_172/after64k.bin').read_bytes()
        after=(R/'Evidence/phase9_oled_followup_20261016_093113/FIXED_V1_FLASH64K_COMPARE_ONLY.bin').read_bytes()
        m.verify_flash(before);m.verify_flash(after,True)
        with self.assertRaises(RuntimeError):m.verify_flash(before,True)
        with self.assertRaises(RuntimeError):m.verify_flash(after)
    def test_status_rejects_trial_and_error(self):
        good=b'IAP STATE=1 ACTIVE=65536 ATTEMPTS=0 DOWNLOAD=0 BACKUP=0 ERROR=0 LOG_ERROR=0\r\n'
        self.assertEqual(m.confirmed(good)['ACTIVE'],65536)
        for bad in [good.replace(b'STATE=1',b'STATE=7'),good.replace(b'LOG_ERROR=0',b'LOG_ERROR=1'),good.replace(b'ACTIVE=65536',b'ACTIVE=131072')]:
            with self.assertRaises(RuntimeError):m.confirmed(bad)
    def test_real_sender_filename_bytes_and_retry(self):
        blob=m.preflight();p=old.Port(nak=True);m.RepairLink(p).ymodem(blob)
        self.assertEqual(p.writes[1][3:-2].split(b'\0',1)[0],b'APP_V1_RELEASE.fw')
        self.assertEqual(bytes(p.data[:len(blob)]),blob);self.assertEqual(p.eots,2)
        data=[x for x in p.writes if len(x)==1029];self.assertEqual(data[0],data[1])
    def test_cancel_or_wrong_blob_stops(self):
        p=old.Port(cancel=True)
        with self.assertRaises(RuntimeError):m.RepairLink(p).ymodem(m.preflight())
        p=old.Port()
        with self.assertRaises(RuntimeError):m.RepairLink(p).ymodem(b'bad')
        self.assertEqual(p.writes,[])
    def test_baseline_rejects_before_serial(self):
        blob=m.preflight()
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'Evidence').mkdir()
            with patch.object(m,'ROOT',root),patch.object(m,'preflight',return_value=blob),patch.object(m,'swd_read',return_value=bytes(65536)),patch.object(m,'with_serial') as serial,patch.object(sys,'argv',['tool','--execute']):
                with self.assertRaises(RuntimeError):m.main()
                serial.assert_not_called()
    def test_swd_read_command_has_no_programming(self):
        class Result:returncode=0;stdout=b'Data read successfully';stderr=b''
        with tempfile.TemporaryDirectory() as td:
            folder=Path(td)
            def run(cmd,**kwargs):
                self.assertEqual(cmd[1:6],['-c','port=SWD','mode=HOTPLUG','freq=950','-u'])
                self.assertFalse(set(cmd)&{'-d','-e','-ob','-reset','-halt','-run'})
                Path(cmd[-1]).write_bytes(b'123');return Result()
            with patch.object(m.subprocess,'run',side_effect=run):self.assertEqual(m.swd_read(folder,'x.bin','0x20000000',3),b'123')
    def test_original_v1_after_transfer_is_not_success(self):
        blob=m.preflight();baseline=(R/'Evidence/phase9_boot_fix_hw_20261016_091007_172/after64k.bin').read_bytes()
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'Evidence').mkdir()
            with patch.object(m,'ROOT',root),patch.object(m,'preflight',return_value=blob),patch.object(m,'swd_read',side_effect=[baseline,baseline]),patch.object(m,'with_serial') as serial,patch.object(sys,'argv',['tool','--execute']),patch('builtins.input') as prompt:
                with self.assertRaises(RuntimeError):m.main()
                self.assertEqual(serial.call_count,1);prompt.assert_not_called()

if __name__=='__main__':unittest.main()

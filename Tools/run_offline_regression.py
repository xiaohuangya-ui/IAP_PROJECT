"""Run maintained host regressions with project .venv; never import hardware runners."""
from pathlib import Path
import argparse
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', required=True, help='New output directory relative to project root')
parser.add_argument('--from-suite', help='Resume at this suite into a new evidence directory')
args = parser.parse_args()
assert Path(sys.executable).resolve() == (ROOT/'.venv/Scripts/python.exe').resolve(), 'Use project .venv Python'
output = ROOT/args.output
output.mkdir(parents=True, exist_ok=False)
suites = [
    ('ymodem_crc_pack', 'test_phase234.py', ['4', '{out}']),
    ('protocol_service', 'test_protocol_service.py', ['{out}']),
    ('router_roles', 'test_router_roles.py', ['{out}']),
    ('uart_connected', 'test_uart_ll_connected.py', ['{out}']),
    ('storage', 'test_storage.py', ['{out}', '6']),
    ('journal_trial_rollback', 'test_storage.py', ['{out}', '8fsm']),
    ('boot_policy', 'test_product_flow.py', ['{out}']),
    ('lsi_handoff', 'test_lsi.py', ['{out}']),
    ('iwdg_start', 'test_iwdg_start_fix.py', ['{out}']),
    ('app_model', 'test_phase9.py', ['{out}']),
    ('app_runtime', 'test_phase9_runtime.py', ['{out}']),
    ('app_bkp', 'test_phase9_bkp.py', ['{out}']),
    ('app_history', 'test_app_history.py', ['{out}']),
    ('app_history_runtime', 'test_phase9_runtime.py', ['{out}', '--history']),
]
rows = []
if args.from_suite:
    names = [suite[0] for suite in suites]
    if args.from_suite not in names:
        parser.error('Unknown suite')
    suites = suites[names.index(args.from_suite):]
for name, script, arguments in suites:
    target = (Path(args.output)/name).as_posix()
    command = [sys.executable, str(ROOT/'Tools'/script)] + [a.replace('{out}', target) for a in arguments]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=240)
    (output/(name+'.driver.log')).write_bytes(result.stdout+result.stderr)
    rows.append({'name': name, 'command': command, 'returncode': result.returncode})
    (output/'results.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    print(name, 'PASS' if result.returncode == 0 else 'FAIL', flush=True)
    result.check_returncode()

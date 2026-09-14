"""Opening-level authored NPC Slay_Object, including a repeated request."""
import json, os, struct, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/script-slay'
folder.mkdir(exist_ok=True)
source = folder / 'inputs-90.bin'
source.write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(90 * 44))
cases = []
for name, setup in [('control', None), ('repeat', '9362,9362')]:
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp')
    if setup:
        env['RF_REPLAY_SETUP_UID'] = setup
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / f'{name}.ppm')],
        env=env, capture_output=True, text=True)
    (folder / f'{name}.log').write_text(run.stdout + run.stderr)
    assert run.returncode == 0, run.stderr[-1000:]
    state = list(map(int, next(x for x in run.stdout.splitlines() if x.startswith('SCRIPT_SLAYS ')).split()[1:]))
    if setup:
        assert state[:3] == [1, 1, 8432] and state[4:] == [0, 0], state
        assert struct.unpack('<f', struct.pack('<I', state[3]))[0] <= 0, state
    else:
        assert state == [0] * 6, state
    cases.append(dict(name=name, state=state))
report = dict(result='PASS', scope='Explicit authored NPC slay; repeated request does not re-enter death.', cases=cases)
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)

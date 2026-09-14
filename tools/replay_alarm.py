"""Process-local authored Alarm46 activation, timeout, switch-off and NPC wake."""
import json, os, struct, subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/alarm-replay'
folder.mkdir(exist_ok=True)
rows = []
cases = [
    ('on', 'L1S1.rfl', 120, '8686'),
    ('repeat', 'L1S1.rfl', 120, '8686,8686'),
    ('switch_off', 'L1S1.rfl', 580, '8686,8687'),
    ('timeout', 'L1S1.rfl', 1080, '8686'),
    ('linked_lab', 'L8S1.rfl', 120, '6449'),
]
for name, level, frames, setup in cases:
    source = folder / (name + '.bin')
    source.write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(frames * 48))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_SETUP_UID=setup)
    if level == 'L8S1.rfl':
        env['RF_REPLAY_ARCHIVE'] = 'levels2.vpp'
    run = subprocess.run([
        str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / (name + '.ppm')),
    ], cwd=root, env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    words = list(map(int, next(s for s in run.stdout.splitlines() if s.startswith('ALARM ')).split()[1:]))
    assert f'Completed {frames} frames' in run.stdout
    assert words[2] == 1 and words[9] == 0, words
    if name in ('on', 'repeat'):
        assert words[0] == (2 if name == 'repeat' else 1) and words[3:6] == [0, 1, 17000], words
    elif name == 'switch_off':
        assert words[0:6] == [1, 1, 1, 1, 0, 17000], words
    elif name == 'timeout':
        assert words[0:6] == [1, 0, 1, 1, 0, 0xffffffff], words
    else:
        assert words[0] == 1 and words[6:9] == [1, 1, 1], words
    rows.append(dict(case=name, level=level, frames=frames, alarm=words))
    print(rows[-1], flush=True)
(folder / 'report.json').write_text(json.dumps(dict(
    result='PASS', cases=rows,
    scope='Authored events activated through process-local setup; siren ownership, timer, delayed switch-off and linked NPC wake. Exact original AI states and audible device output are not proved.',
), indent=2))

"""Campaign custom-animation scheduling, completion and movement cancellation."""
import json, os, struct, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/script-animation-replay'
folder.mkdir(exist_ok=True)
rows = []
for name, level, frames, setup, move, expected in [
    ('natural-cower', 'L2S1.rfl', 120, None, None, [1,1,1,0,0,7201,7192,90,0,0]),
    ('action', 'L2S2a.rfl', 600, '8495', None, [6,1,0,1,0,8495,5458,159,1,0]),
    ('cancel', 'L2S2a.rfl', 600, '5459', '8481', [6,1,1,0,0,5459,5458,311,0,1]),
    ('freeze-request', 'L2S2a.rfl', 600, '8539', None, [6,1,0,1,0,8539,5535,599,0,0]),
]:
    source = folder / (name + '.bin')
    source.write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(48 * frames))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_LEVEL'] = level
    if setup: env['RF_REPLAY_SETUP_UID'] = setup
    if move: env['RF_REPLAY_GOTO_UID'] = move
    result = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / (name + '.ppm'))],
        cwd=root, env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(result.stdout + result.stderr)
    result.check_returncode()
    row = list(map(int, next(line for line in result.stdout.splitlines()
        if line.startswith('SCRIPT_ANIMATION ')).split()[1:]))
    assert row == expected, (name, row, expected)
    assert f'Completed {frames} frames' in result.stdout
    rows.append(dict(case=name, animation=row));print(rows[-1], flush=True)
(folder / 'report.json').write_text(json.dumps(dict(result='PASS', cases=rows,
    scope='First case uses authored startup and delay; others explicitly fire existing animation/movement events. Checks scheduling and ownership counters, not visual parity or a full mission.'), indent=2))

"""Use-gated L2S1 airlock exit, with normal contact and opposite-door interlock."""
import json, os, struct, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/airlock-replay'
folder.mkdir(exist_ok=True)
rows = []
for name, use, level, uid, target, exit_frame, room, trigger in [
    ('no-use', 0, 'L2S1.rfl', 3218, 'L2S2a.rfl', 274, 3229, 3226),
    ('use', 1, 'L2S1.rfl', 3218, 'L2S2a.rfl', 274, 3229, 3226),
    ('return-use', 1, 'L2S2a.rfl', 7639, 'L2S1.rfl', 272, 7650, 7646),
]:
    source = folder / (name + '.bin')
    source.write_bytes(b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, float(i >= 30), 0, 0, 0, 0,
                    use if i >= 30 else 0, 0, 0, 0, 0) for i in range(360)))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_EXIT_START=str(uid))
    result = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
        '--spawn-replay', str(root / 'Installed_Game'), str(source),
        str(folder / (name + '.ppm'))], cwd=root, env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(result.stdout + result.stderr)
    result.check_returncode()
    def values(label):
        return [line.split()[1:] for line in result.stdout.splitlines() if line.startswith(label + ' ')]
    transitions = values('LEVEL_TRANSITION')
    if use:
        assert transitions == [[level, target, str(uid), str(exit_frame)]], transitions
        gate = list(map(int, values('LEVEL_EXIT_AIRLOCK')[0]))
        assert gate[1] == 1 and gate[2] > 0 and gate[3] == 0 and gate[4:] == [room, trigger], gate
    else:
        assert not transitions and values('AIRLOCK') == [['0'] * 6]
        gate = [0] * 6
    assert 'Completed 360 frames' in result.stdout and values('PLAYER_LIFE')[0][0] == '0'
    rows.append(dict(case=name, transitions=transitions, departure_interlock=gate))
    print(rows[-1], flush=True)
(folder / 'report.json').write_text(json.dumps(dict(result='PASS', cases=rows,
    scope='One placement outside airlock B, walking and held Use; no forced event. Checks initial peer-cycle blocking and eventual exit, not a full chamber traversal or pressure simulation.'), indent=2))

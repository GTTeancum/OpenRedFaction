"""Walk across authored section boundaries after one initial camera/player placement."""
import json, os, struct, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/area2-routes'
folder.mkdir(exist_ok=True)
source = folder / 'walk.bin'
source.write_bytes(b'RFI6' + struct.pack('<I', 48) + b''.join(
    struct.pack('<5f7I', 0, 0, float(i >= 30), 0, 0, 0, 0, 0, 0, 0, 0, 0)
    for i in range(240)))
rows = []
for name, level, uid, target, offset in [
    ('section3', 'L2S2a.rfl', 5150, 'L2S3.rfl', (63, 4, 15.5)),
    ('area3', 'L2S3.rfl', 6604, 'L3S1.rfl', (-167, -70.96875, -59.25)),
    ('return', 'L3S1.rfl', 66, 'L2S3.rfl', (167, 70.96875, 59.25)),
]:
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_EXIT_START=str(uid))
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / (name + '.ppm'))],
        cwd=root, env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def lines(label):
        return [s.split()[1:] for s in run.stdout.splitlines() if s.startswith(label + ' ')]
    transition = lines('LEVEL_TRANSITION')
    assert len(transition) == 1 and transition[0][:3] == [level, target, str(uid)], transition
    assert 30 < int(transition[0][3]) < 120
    departure, arrival = list(map(float, lines('LEVEL_EXIT_POSE')[0])), list(map(float, lines('LEVEL_ARRIVAL')[0]))
    assert all(abs(arrival[i] - departure[i] - offset[i]) < .00005 for i in range(3)), (departure, arrival)
    body = list(map(int, lines('PC_PLAY_BODY')[0]))
    position = struct.unpack('<3f', struct.pack('<3I', *body[22:25]))
    assert sum((position[i] - arrival[i]) ** 2 for i in range(3)) > 1, position
    assert lines('PLAYER_LIFE')[0][0] == '0' and 'Completed 240 frames' in run.stdout
    rows.append(dict(case=name, transition=transition[0], departure=departure,
        arrival=arrival, final_position=position))
    print(rows[-1], flush=True)
(folder / 'report.json').write_text(json.dumps(dict(result='PASS', cases=rows,
    scope='One initial placement outside each exit; real movement/contact, one transition, translated arrival and continued movement. No forced event/exit. These are separate boundary fixtures, not a full walked campaign.'), indent=2))

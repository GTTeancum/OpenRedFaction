"""L2S3 frame progression, colliding particles and instantaneous PONR mover."""
import json, os, struct, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/l2s3-replay'
folder.mkdir(exist_ok=True)
source = folder / 'neutral.bin'
source.write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(120 * 48))
cases = []
for name, setup in [('natural', None), ('alarm', '2102')]:
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S3.rfl', RF_REPLAY_ARCHIVE='levels1.vpp')
    if setup:
        env['RF_REPLAY_SETUP_UID'] = setup
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / (name + '.ppm'))],
        cwd=root, env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def words(label):
        return list(map(int, next(s for s in run.stdout.splitlines() if s.startswith(label + ' ')).split()[1:]))
    particles, motion, alarm = words('SCENE_PARTICLES'), words('LIVE_MOTION'), words('ALARM')
    assert 'Completed 120 frames' in run.stdout
    assert particles[0] == 119 and particles[3] > 0 and particles[4] > 0 and particles[6] > 0, particles
    assert motion[1] == 119 and motion[4] >= 1 and motion[7] == 0, motion
    if setup:
        assert alarm[0] == 1 and alarm[2] == 1 and alarm[4:6] == [1, 17250] and alarm[9] == 0, alarm
    else:
        assert alarm[0] == 0 and alarm[4] == 0, alarm
    cases.append(dict(case=name, particles=particles, motion=motion, alarm=alarm))
    print(cases[-1], flush=True)
(folder / 'report.json').write_text(json.dumps(dict(result='PASS', cases=cases,
    scope='Two seconds at authored spawn; live particles, mover progression and optional authored alarm setup. Not a walked campaign section.'), indent=2))

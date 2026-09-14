"""Exercise held baton fire, battery exhaustion/reload and primary fallback on PC."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/riot-alternate'
folder.mkdir(exist_ok=True)
results = []
for name, frames, near, stop, reload, setup in [
    ('held_near', 180, True, 180, -1, '9870'),
    ('released', 210, True, 150, -1, '9870'),
    ('empty_bash', 360, False, 330, -1, '9870'),
    ('replace_battery', 360, False, 150, 160, '9870,9870'),
    ('both_buttons', 150, True, 121, -1, '9870'),
]:
    records = []
    for frame in range(frames):
        movement = (1 if near else -1) if 10 <= frame < (25 if near else 75) else 0
        primary = frame == (120 if name == 'both_buttons' else 330) if name in ('both_buttons', 'empty_bash') else False
        records.append(struct.pack('<5f7I', 0, 0, movement, 0, 0, 0, 0, 0,
            primary, frame == reload, frame == 5, 120 <= frame < stop))
    source = folder / (name + '.bin')
    source.write_bytes(b'RFI6' + struct.pack('<I', 48) + b''.join(records))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ACTOR_UID='8456', RF_REPLAY_SETUP_UID=setup)
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / (name + '.ppm'))],
        cwd=root, env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def words(label):
        return list(map(int, next(s for s in run.stdout.splitlines() if s.startswith(label + ' ')).split()[1:]))
    combat, ammo, baton, model, audio = map(words, ('COMBAT', 'PLAYER_AMMO', 'RIOT_STICK', 'PLAYER_WEAPON', 'WEAPON_AUDIO'))
    assert combat[7] == ammo[7] == baton[7] == model[6] == audio[7] == 0, (combat, ammo, baton, model, audio)
    assert ammo[0] == 2 and model[2] > 0 and model[4] <= 1024 * 1024
    if name == 'held_near':
        assert baton[:3] == [1, 60, 40] and baton[3] > 0 and baton[4] > 0, baton
        assert ammo[:3] == [2, 0, 60] and model[1] == 3, (ammo, model)
    elif name == 'released':
        assert baton[:3] == [0, 30, 20] and ammo[:3] == [2, 0, 80] and model[1] == 0, (baton, ammo, model)
    elif name == 'empty_bash':
        assert baton[:4] == [0, 150, 100, 0] and baton[5] > 0 and ammo[:3] == [2, 0, 0], (baton, ammo)
        assert combat[0] == 6 and combat[1] == 0, combat  # Five taser starts, then a free bash.
    elif name == 'replace_battery':
        assert baton[:3] == [0, 30, 20] and baton[6] == 1, baton
        assert ammo[:5] == [2, 0, 100, 100, 1], ammo  # Partly used cell discarded, full spare loaded.
    elif name == 'both_buttons':
        assert baton[:4] == [0, 0, 0, 0] and ammo[:3] == [2, 0, 100], (baton, ammo)
        assert combat[:2] == [1, 1], combat
    assert f'Completed {frames} frames' in run.stdout
    result = dict(case=name, frames=frames, combat=combat, ammo=ammo, baton=baton, model=model, audio=audio)
    results.append(result)
    print(result, flush=True)
(folder / 'report.json').write_text(json.dumps(dict(result='PASS', cases=results,
    scope='Authored item event and process-local actor staging. Actual movement/input, charge conservation, '
          'release, empty-battery bash and reload. Not full campaign or retail effect parity.'), indent=2))

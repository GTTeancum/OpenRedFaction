"""Authored stripping, rearming, unarmed section handoff and respawn."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/weapon-strip'
folder.mkdir(exist_ok=True)
cases = []
for name, count in [('unarmed', 150), ('rearm', 150), ('late_strip', 150),
                    ('return', 240), ('respawn', 1500)]:
    source = folder / (name + '.bin')
    source.write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(
        struct.pack('<5f6I', 0, 0, float(name == 'rearm' and 10 <= i < 25),
                    0, 0, 0, 0, int(name == 'respawn' and i == 1300),
                    int(i in (30, 120, 210)), int(i == 50), int(i == 5))
        for i in range(count)))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    setup = '8366,9870' if name == 'rearm' else '9870,8366' if name == 'late_strip' else '8366'
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ACTOR_UID='8456', RF_REPLAY_SETUP_UID=setup)
    if name == 'return':
        env.update(RF_REPLAY_EXIT_UID='9019', RF_REPLAY_RETURN_EXIT_UID='9346')
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-replay', str(root / 'Installed_Game'),
                          str(source), str(folder / (name + '.ppm'))],
                         env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def words(label):
        return list(map(int, next(s for s in run.stdout.splitlines()
                                 if s.startswith(label + ' ')).split()[1:]))
    ammo, model, combat, life = map(words, ('PLAYER_AMMO', 'PLAYER_WEAPON', 'COMBAT', 'PLAYER_LIFE'))
    assert ammo[7] == model[6] == combat[7] == life[7] == 0
    if name == 'rearm':
        assert ammo[:3] == [2, 0, 1] and model[2] > 0 and combat[:2] == [1, 1]
    else:
        assert ammo[:3] == [0xffffffff, 0, 0] and model[2] == 0, (ammo, model)
        assert combat[0] == int(name == 'late_strip'), combat
    if name == 'return':
        assert [s for s in run.stdout.splitlines() if s.startswith('LEVEL_TRANSITION ')] == [
            'LEVEL_TRANSITION L1S1.rfl L1S2.rfl 9019 61', 'LEVEL_TRANSITION L1S2.rfl L1S1.rfl 9346 181']
    if name == 'respawn':
        assert life[:3] == [1, 1, 0], life
    assert f'Completed {count} frames' in run.stdout
    cases.append(dict(case=name, ammo=ammo, combat=combat, life=life))
report = dict(result='PASS', cases=cases,
              scope='Explicit authored strip/grant and exit events with staged actor view. '
                    'No firing while unarmed, no weapon vertices, automatic supported rearm, '
                    'unarmed section return and respawn. Not natural campaign traversal.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)

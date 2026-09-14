"""Opening campaign contacts and delayed Riot Stick handoff; no event injection."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-handoff'
folder.mkdir(exist_ok=True)
results = []
for case, frames, approach in [('hall', 2400, False), ('before_grant', 1750, True), ('handoff', 2400, True)]:
    records = []
    for i in range(frames):
        x = z = 0
        if 30 <= i < 210:
            z = 1
        if 240 <= i < 470:
            x, z = -.8239215, .5667039
        if approach and 500 <= i < 720:
            x, z = .1145, .9934
        records.append(struct.pack('<5f6I', x, 0, z, 0, 0, 0, 0, 0, 0, 0, 0))
    source = folder / (case + '.bin')
    source.write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(records))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_EXIT_START='9646')
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / (case + '.ppm'))],
        env=env, capture_output=True, text=True)
    (folder / (case + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def words(label):
        return list(map(int, next(s for s in run.stdout.splitlines()
            if s.startswith(label + ' ')).split()[1:]))
    grants, ammo, contacts = words('SCRIPT_GRANTS'), words('PLAYER_AMMO'), words('TRIGGER_CONTACTS')
    body = words('PC_PLAY_BODY')
    position = struct.unpack('<3f', struct.pack('<3I', *body[22:25]))
    assert words('PLAYER_LIFE')[0] == 0 and contacts[-1] == 0
    assert f'Completed {frames} frames' in run.stdout
    if case == 'handoff':
        assert grants == [1, 1, 1, 2, 1, 1, 0, 0], grants
        assert ammo[:4] == [2, 0, 1, 0], ammo
        assert contacts[2] == 9869, contacts
        assert words('SCRIPT_SLAYS')[:2] == [2, 2]
    else:
        assert grants == [0] * 8 and ammo[0] == 0xffffffff, (case, grants, ammo)
    results.append(dict(case=case, frames=frames, grants=grants, ammo=ammo, position=position, contacts=contacts))
report = dict(result='PASS', cases=results,
    scope='Staged once outside player trigger9029. Ordinary walking crosses9029/9028 and reaches9869. '
          'No forced event, grant, slay or later reposition. Hall control does not enter the grant volume; '
          '1750-frame approach remains unarmed pending the scripted delay; final2400-frame approach receives Riot Stick. '
          'This proves mission contacts/inventory delivery, not finished confrontation animations or a full campaign.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)

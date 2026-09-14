"""Process-local authored medical/repair pickup checks, with actual NPC damage."""
import json
import os
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/vital-pickups'
folder.mkdir(exist_ok=True)
rows = []
for name, uid, start, frames, restored_slot in [
    ('health_full', 9789, 0, 90, None),
    ('health', 9789, 200, 300, 2),
    ('armor', 9868, 200, 300, 3),
]:
    source = folder / (name + '.bin')
    source.write_bytes(b'RFI4' + struct.pack('<I', 40) + b''.join(
        struct.pack('<5f5I', 0, 0, float(start <= i < start + 15), 0, 0, 0, 0, 0, 0, 0)
        for i in range(frames)))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_ITEM_UID=str(uid), RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp')
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / (name + '.ppm'))],
        env=env, capture_output=True, text=True, check=True)
    (folder / (name + '.txt')).write_text(run.stdout + run.stderr)
    def words(key):
        return list(map(int, next(x for x in run.stdout.splitlines() if x.startswith(key + ' ')).split()[1:]))
    pickups, enemy = words('PICKUPS'), words('ENEMY_COMBAT')
    vitals = [struct.unpack('<f', struct.pack('<I', x))[0] for x in words('PICKUP_VITALS')]
    assert pickups[7] == enemy[7] == 0 and pickups[0] == 7
    assert all(0 <= x <= 100 for x in vitals[:2]), vitals
    if restored_slot is None:
        assert pickups[3] == 0 and pickups[1] > 0 and pickups[6] > 0 and vitals == [100, 100, 0, 0]
    else:
        assert enemy[3] > 0 and pickups[3] == 1 and pickups[5] == uid
        assert 0 < vitals[restored_slot] <= 25 and vitals[5 - restored_slot] == 0, vitals
    rows.append(dict(name=name, pickups=pickups, vitals=vitals, enemy=enemy))
    print(rows[-1], flush=True)
(folder / 'report.json').write_text(json.dumps(dict(result='PASS', cases=rows), indent=2))

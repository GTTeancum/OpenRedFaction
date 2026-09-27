"""Text-only L1S1 ground-motion save and fresh-process continuation."""

import os
from pathlib import Path
import re
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
route = (folder / 'postwall-bridge-cover.bin').read_bytes()
header = b'RFI6' + struct.pack('<I', 48)
assert route[:8] == header and (len(route) - 8) // 48 >= 4501
prefix = folder / 'moving-ground-capture.bin'
suffix = folder / 'moving-ground-resume.bin'
prefix.write_bytes(route[:8 + 4321 * 48])
# Local frame zero initializes the fresh scene before applying the save.
suffix.write_bytes(header + route[8 + 4320 * 48:8 + 4501 * 48])
base = folder / 'moving-ground-save'
env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp')


def run(name, replay, extra):
    result = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                             '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                             str(replay)], cwd=root, env=dict(env, **extra),
                            capture_output=True, text=True)
    output = result.stdout + result.stderr
    (folder / f'moving-ground-{name}.log').write_text(output)
    result.check_returncode()
    return output


captured = run('capture', prefix, dict(RF_REPLAY_WORLD_SNAPSHOT_OUT=str(base)))
match = re.search(r'WORLD_SNAPSHOT_STORED bytes\d+ slot([01]) ', captured)
assert match, 'moving player save was rejected'
data = Path(f'{base}.{match.group(1)}').read_bytes()
at = data.find(b'RFPL')
assert at >= 0 and struct.unpack_from('<I', data, at + 4)[0] == 4
assert struct.unpack_from('<f', data, at + 68)[0] > 5
loaded = run('loaded', suffix, dict(RF_REPLAY_WORLD_SNAPSHOT_IN=str(base)))
assert 'WORLD_SNAPSHOT_LOADED ' in loaded and 'PLAYER_LIFE 0 ' in loaded
assert 'PICKUP_VITALS 1097439636 1085066448 ' in loaded
line = next(line for line in loaded.splitlines()
            if line.startswith('CAMPAIGN_FINAL_POSITION '))
position = tuple(map(float, line.split()[1:4]))
reference = (-1.006042, 3.482136, 37.866348)
assert max(abs(a - b) for a, b in zip(position, reference)) < .15, position
print('PASS moving-ground save/load; version4, alive, guard-hit vitals, position',
      position)

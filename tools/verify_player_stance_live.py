"""Check campaign stance timing with process-local input, without host input."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root/'artifacts/player-stance-live'
folder.mkdir(exist_ok=True)
source = folder/'input.bin'
source.write_bytes(b''.join(struct.pack('<5fI', 0, 0, .5 if 24 <= i < 40 else 0,
                                      0, 0, int(8 <= i < 40)) for i in range(64)))
exe = root/'build/pc/Release/rf_pc_play.exe'
run = subprocess.run([str(exe), '--spawn-replay', str(root/'Installed_Game'),
                      str(source), str(folder/'final.ppm')], cwd=root,
                     capture_output=True, text=True, check=True)
(folder/'pc.txt').write_text(run.stdout)
rows = {line.split()[0]: list(map(int, line.split()[1:]))
        for line in run.stdout.splitlines() if line.startswith('PLAYER_')}
stance = [rows['PLAYER_STANCE_FRAMES'][i*8:i*8+8] for i in range(64)]
motion = [rows['PLAYER_MOTION_FRAMES'][i*12:i*12+12] for i in range(64)]
assert stance[7][5] & 0x400 == 0
assert stance[8][2:7] == [1, 0, 0, 0x400, 0], 'Crouch must take effect on press'
assert all(row[5] & 0x400 for row in stance[8:40])
assert all(row[6] == 9 for row in motion[8:24])
assert all(row[6] == 10 for row in motion[24:40])
assert stance[40][2:7] == [2, 0, 0x400, 0, 0], 'Clear standing must take effect on release'
assert all(row[5] & 0x400 == 0 for row in stance[40:])
assert all(row[6] == 1 for row in motion[40:])
assert all(row[0] == 2 for row in motion[1:])
report = dict(result='PASS', frames=64, press_tick=8, move_tick=24, release_tick=40,
              pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
              scope='Campaign fixture immediate crouch, crouched motion and unobstructed release. Does not verify blocked standing, full ownership gates or physical controller input.')
(folder/'report.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))

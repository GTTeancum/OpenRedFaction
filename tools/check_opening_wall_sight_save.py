import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
game = root / 'build/pc/Release/rf_pc_play.exe'
route = (folder / 'postwall-bridge-cover.bin').read_bytes()
header = b'RFI6' + struct.pack('<I', 48)
assert route[:8] == header
suffix = folder / 'wall-save-sight-suffix.bin'
suffix.write_bytes(header + route[8 + 4245 * 48:8 + 4410 * 48])
base = folder / 'wall-save/sight-phase'
env = {key: val for key, val in os.environ.items() if not key.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp')
def run(label, path, extra):
    result = subprocess.run([str(game), '--spawn-telemetry-replay',
                             str(root / 'Installed_Game'), str(path)],
                            cwd=root, env=dict(env, **extra),
                            capture_output=True, text=True)
    output = result.stdout + result.stderr
    (folder / f'wall-save-sight-{label}.log').write_text(output)
    print(label, 'exit', result.returncode)
    for line in output.splitlines():
        if line.startswith(('WORLD_SNAPSHOT_STORED ', 'WORLD_SNAPSHOT_LOADED ',
                            'CAMPAIGN_FINAL_POSITION ', 'PLAYER_LIFE ',
                            'PICKUP_VITALS ', 'ENEMY_COMBAT ')):
            print(line)
    result.check_returncode()
    return output
run('capture', folder / 'wall-save/wall-cross-save.bin',
    dict(RF_REPLAY_WORLD_SNAPSHOT_OUT=str(base)))
loaded = run('loaded', suffix, dict(RF_REPLAY_WORLD_SNAPSHOT_IN=str(base),
                                    RF_REPLAY_TRACE='1', RF_REPLAY_TRACE_FROM='0'))
assert 'WORLD_SNAPSHOT_LOADED ' in loaded
assert 'ENEMY_SHOT_TRACE 107 8462' in loaded
assert 'ENEMY_SHOT_TRACE 137 8462' in loaded
assert 'PLAYER_LIFE 0 ' in loaded
assert 'PICKUP_VITALS 1097439636 1085066448 ' in loaded
print('PASS: saved wall route survives the first two guard shots with uninterrupted-route vitals')

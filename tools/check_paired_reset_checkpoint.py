"""Paired reset save/reload followed by a real rocket, with uninterrupted control.

Default inputs come from check_paired_authored_reset.py; --connected uses
check_beam_continuation.py --connected. No HDD images or host input are used;
all replay commands stay inside rf_pc_play.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--connected', action='store_true', help='Reset beam95/post94 after a shared rocket blast')
parser.add_argument('--settle', action='store_true', help='Save airborne recut debris at frame1020 and continue through frame1450')
args = parser.parse_args()
folder = ROOT / ('artifacts/connected-reset-checkpoint' if args.connected else 'artifacts/paired-reset-checkpoint')
folder.mkdir(parents=True, exist_ok=True)
(folder / 'report.json').unlink(missing_ok=True)
if args.connected:
    reset = bytearray((ROOT / 'artifacts/connected-beam-live/save.bin').read_bytes())
    assert len(reset) == 8 + 600 * 48
    reset += bytes(250 * 48)
    for frame in range(730,770): struct.pack_into('<I',reset,8+48*frame+20,1)
    for offset in (28,44): struct.pack_into('<I',reset,8+48*750+offset,1)
    recut = bytearray(reset) + bytes(300 * 48)
    struct.pack_into('<I',recut,8+48*940+32,1)
else:
    reset = (ROOT / 'artifacts/paired-reset/reset.bin').read_bytes()
    recut = (ROOT / 'artifacts/paired-reset/recut.bin').read_bytes()
assert reset[:8] == b'RFI6' + struct.pack('<I', 48)
assert len(reset) == 8 + 850 * 48 and len(recut) == 8 + 1150 * 48
assert recut[:len(reset)] == reset
# The reset run executes records 1..849. Resume record 0 is initialization;
# its records 1..300 reproduce uninterrupted records 850..1149.
resume = reset[:8] + bytes(48) + recut[len(reset):]
env = {k: v for k, v in os.environ.items()
       if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1', RF_REPLAY_AUTHORED_SOURCES='2',
           RF_REPLAY_PLAYER_CHECKPOINT='1')
if args.connected: env['RF_REPLAY_AUTHORED_SOURCE']='95'


def run(name, payload, load=None):
    inputs = folder / (name + '.bin')
    inputs.write_bytes(payload)
    output = folder / (name + '.rfcp')
    output.unlink(missing_ok=True)
    local = dict(env, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(output))
    if load is not None:
        local['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(load)
    with (folder / (name + '.log')).open('wb') as log:
        result = subprocess.run([
            str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
            str(ROOT / 'Installed_Game'), str(inputs), str(folder / (name + '.ppm')),
        ], cwd=ROOT, env=local, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0 and output.exists(), f'Inspect {name}.log'
    data = output.read_bytes()
    assert data[:4] == b'RFCP' and struct.unpack_from('<I', data, 16)[0] == 3
    assert data[576:580] == b'RFDS' and struct.unpack_from('<I', data, 580)[0] == 3
    lines = (folder / (name + '.log')).read_text(encoding='utf-8').splitlines()
    def last(label):
        return list(map(int, [l for l in lines if l.startswith(label + ' ')][-1].split()[1:]))
    return data, {k: last(k) for k in ('AUTHORED_SOURCE_CUTS', 'TERRAIN_PUBLICATION', 'DETACHED_PIECES')}


saved, initial = run('save', reset)
assert initial['AUTHORED_SOURCE_CUTS'] == ([95,0,94,0,0,0,0,0] if args.connected else [94,0,93,0,0,0,0,0])
assert initial['TERRAIN_PUBLICATION'][2:4] == [0, 2 if args.connected else 3]
assert initial['DETACHED_PIECES'][1:3] == [0, 0]
assert struct.unpack_from('<I',saved,576+248)[0] == 0, 'Reset retained old lightmaps'
resumed, final = run('resume', resume, folder / 'save.rfcp')
control, expected = run('control', recut)
assert final['AUTHORED_SOURCE_CUTS'] == ([95,1,94,1,0,0,0,0] if args.connected else [94,0,93,1,0,0,0,0])
assert final['TERRAIN_PUBLICATION'][2:4] == ([2,3] if args.connected else [1,4])
assert final['DETACHED_PIECES'][1:3] == ([1,3] if args.connected else [1,1])
assert resumed == control, 'Reset-save continuation differs'
for label in final:
    # Publication peak measures process history: the control also performed
    # the two pre-reset cuts. Compare live state, not that high-water mark.
    indices = (0, 1, 2, 3, 4, 6, 7) if label == 'TERRAIN_PUBLICATION' else range(len(final[label]))
    assert all(final[label][i] == expected[label][i] for i in indices), label
report = dict(result='PASS', connected=args.connected, save_bytes=len(saved), continuation_bytes=len(resumed),
              reset=initial, recut=final,
              scope='PC paired reset save/reload and subsequent real rocket match uninterrupted composed state exactly; native coverage is separate.')
if args.settle:
    airborne, _ = run('airborne', recut[:8 + 1020 * 48])
    continued, state = run('settle', reset[:8] + bytes(431 * 48), folder / 'airborne.rfcp')
    uninterrupted, _ = run('settle-control', recut + bytes(300 * 48))
    assert continued == uninterrupted, 'Moving-debris save continuation differs'
    def motion(name):
        lines = (folder / (name + '.log')).read_text(encoding='utf-8').splitlines()
        return list(map(int, [line for line in lines if line.startswith('DETACHED_MOTION ')][-1].split()[1:]))
    before, after = motion('airborne'), motion('settle')
    assert after[0] == before[0], 'Debris disappeared during continuation'
    assert after[6] == 0, 'Debris motion failed'
    if args.connected:
        assert before[0] == 3 and before[3] < 3, 'Expected moving fragments at save'
        assert after[3] == 3, 'Not all three fragments settled'
        # RFPBv2: 16-byte header, 328-byte records, 12-byte record key;
        # explicit body codec puts position at byte88. This room's floor is Y=-1.5.
        pieces = continued.index(b'RFPB')
        assert struct.unpack_from('<III', continued, pieces + 4) == (2, 1000, 3)
        for i in range(3):
            position = struct.unpack_from('<3f', continued, pieces + 16 + i * 328 + 12 + 88)
            assert position[1] >= -1.5, f'Fragment {i} fell below the room floor: {position}'
    report['settle'] = dict(bytes=len(continued), before=before, after=after, state=state)
(folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, indent=2))

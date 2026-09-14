"""Compare a preserved pre-cache PC executable with current world projection."""
import argparse, hashlib, json, os, struct, subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('baseline', type=Path)
a = p.parse_args()
r = Path(__file__).resolve().parents[1]
d = r / 'artifacts/world-cache'
d.mkdir(exist_ok=True)
executables = [a.baseline.resolve(), r / 'build/pc/Release/rf_pc_play.exe']
hashes = [hashlib.sha256(x.read_bytes()).hexdigest() for x in executables]
assert hashes[0] != hashes[1], 'Must compare distinct executable builds'
source = d / 'walk.bin'
source.write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(
    struct.pack('<5f6I', 0, 0, float(i >= 30), 0, 0, 0, 0, 0, 0, 0, 0)
    for i in range(180)))
still = d / 'still.bin'
still.write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(120 * 44))
cases = [('forward', 'L1S1.rfl', '9019'), ('backward', 'L1S2.rfl', '9346'),
         ('section3', 'L1S2.rfl', '9512'), ('section2', 'L1S3.rfl', '9324'),
         ('area2', 'L1S3.rfl', '7079'), ('actors', 'L1S1.rfl', None)]
prefixes = ('ACTOR_FOLLOW_SUMMARY ', 'NPC_DRAW ', 'CLUTTER_DRAW ',
            'WEAPON_DRAW ', 'PLAYER_WEAPON ', 'PC_PLAY_BODY ',
            'LEVEL_TRANSITION ', 'LEVEL_ARRIVAL ')
results = []
for name, level, uid in cases:
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_LEVEL'] = level
    env['RF_REPLAY_EXIT_START' if uid else 'RF_REPLAY_ACTOR_UID'] = uid or '8323'
    runs, images = [], []
    for mode, exe in zip(('baseline', 'cached'), executables):
        output = d / f'{name}-{mode}.ppm'
        run = subprocess.run([str(exe), '--spawn-replay', str(r / 'Installed_Game'),
                              str(source if uid else still), str(output)],
                             env=env, capture_output=True, text=True)
        (d / f'{name}-{mode}.log').write_text(run.stdout + run.stderr)
        run.check_returncode()
        rows = [x for x in run.stdout.splitlines() if x.startswith(prefixes)]
        assert any(x.startswith('ACTOR_FOLLOW_SUMMARY ') for x in rows)
        assert any(x.startswith('PC_PLAY_BODY ') for x in rows)
        if uid:
            assert sum(x.startswith('LEVEL_TRANSITION ') for x in rows) == 1
        runs.append(rows)
        images.append(output.read_bytes())
    assert runs[0] == runs[1], (name, 'world/model/state summaries changed')
    assert images[0] == images[1], (name, 'final pixels changed')
    results.append(dict(case=name, pixels_identical=True, summaries=runs[1]))
    print(name, 'PASS', flush=True)
report = dict(result='PASS', executable_sha256=hashes, cases=results,
              scope='Five walking transitions and one visible-actor camera; exact final pixels and selected world/model/player summaries. Not exhaustive campaign coverage.')
(d / 'report.json').write_text(json.dumps(report, indent=2))

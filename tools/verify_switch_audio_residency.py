"""Authored campaign Switch audio preparation; playback is a separate gate."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/switch-audio-residency'
folder.mkdir(exist_ok=True)
source = folder / 'inputs.bin'
source.write_bytes(b'RFI3' + struct.pack('<I', 32) + bytes(2 * 32))
env = dict(os.environ)
for key in ('RF_REPLAY_REGION_START', 'RF_REPLAY_DOOR_START',
            'RF_REPLAY_LIFT_START', 'RF_REPLAY_FORCE_UID'):
    env.pop(key, None)
authored = json.loads((root / 'artifacts/events.json').read_text())['results']
switches = [row for level in authored for row in level['records'] if row['type_index'] == 32]
assert len(switches) == 83 and all(not row['texts'][0] for row in switches)
results = []
for archive, name in [('levels1.vpp', 'L1S2.rfl'), ('levels1.vpp', 'L1S3.rfl'),
                      ('levels1.vpp', 'L4S5.rfl'), ('levels2.vpp', 'L9S3.rfl')]:
    rows = next(level['records'] for level in authored
                if level['archive'] == archive and level['file'] == name)
    rows = [row for row in rows if row['type_index'] == 32]
    reject = int(any(int(row['values'][0]) in (1, 2) for row in rows))
    env.update(RF_REPLAY_LEVEL=name, RF_REPLAY_ARCHIVE=archive)
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-replay', str(root / 'Installed_Game'),
                          str(source), str(folder / (name + '.ppm'))],
                         env=env, capture_output=True, text=True)
    (folder / (name + '.txt')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def words(label):
        return list(map(int, next(line for line in run.stdout.splitlines()
                                 if line.startswith(label + ' ')).split()[1:]))
    state, bank, audio = words('SWITCH_AUDIO'), words('SOUND_BANK'), words('LIVE_AUDIO')
    assert state[:3] == [len(rows), 0, reject], (name, state)
    assert (state[3] > 0) == bool(reject), (name, state)
    assert audio[3] == 0 and bank[2] + bank[3] == audio[1] <= 1024 * 1024, (name, audio, bank)
    results.append(dict(level=name, archive=archive, switch_audio=state, sound_bank=bank))
report = dict(result='PASS', results=results,
              scope='All three authored rejection-mode levels and one no-rejection control. '
                    'Actual PC campaign loading and PCM residency after archive close; '
                    'no live Switch activation, rejection playback or original preload-order claim.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))

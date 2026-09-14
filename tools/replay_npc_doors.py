"""Authored L7S2 NPC visibility and hangar-door prerequisites, process-local PC replay."""
import json, os, struct, subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/npc-doors'
folder.mkdir(exist_ok=True)
source = folder / 'inputs-1000.bin'
source.write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(44 * 1000))
cases = []
for name, setup in [('hidden', '4961'), ('door-disabled', '4953'), ('enabled', '4953,4961')]:
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L7S2.rfl', RF_REPLAY_ARCHIVE='levels2.vpp',
               RF_REPLAY_GOTO_UID='4994', RF_REPLAY_SETUP_UID=setup)
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                          str(root / 'Installed_Game'), str(source), str(folder / f'{name}.ppm')],
                         env=env, capture_output=True, text=True)
    (folder / f'{name}.log').write_text(run.stdout + run.stderr)
    assert run.returncode == 0, run.stderr[-1000:]
    def words(label):
        return list(map(int, next(x for x in run.stdout.splitlines() if x.startswith(label + ' ')).split()[1:]))
    contacts, movement, actor, activation = [words(x) for x in
        ('NPC_TRIGGERS', 'SCRIPT_MOVE', 'SCRIPT_ACTOR', 'LIVE_ACTIVATION')]
    position = struct.unpack('<3f', struct.pack('<3I', *actor[1:4]))
    assert contacts[5] == movement[7] == activation[5] == 0
    assert actor[0] == 4952
    if name == 'enabled':
        assert position[0] > 20 and movement[3] == 0, (position, movement)
        assert contacts[1] >= 1 and contacts[2] == 4952 and contacts[4] > 0, contacts
        assert activation[1:3] == [2, 2], activation
        assert 2.8 < position[1] < 3.1, position  # Floor support, below spawn Y3.2067.
    else:
        assert position[0] < 17.5 and activation[2] == 0, (position, activation)
    cases.append(dict(name=name, setup=setup, position=position, contacts=contacts,
                      movement=movement, actor=actor, activation=activation))
report = dict(result='PASS', scope='Explicit authored setup and Goto dispatch; hangar traversal, not automatic cutscene or complete mission progression.', cases=cases)
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report))

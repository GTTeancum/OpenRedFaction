"""Exact process-local RFCP restart, then pure rejection of malformed candidates.

Uses the authored two-blast DEV recording and actual player/body owners.
No build, emulator launch, host input, or original-game execution.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    run = ROOT / 'artifacts/rfcp-gameplay' / datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    run.mkdir(parents=True)
    exe = ROOT / 'build/pc/Release/rf_pc_play.exe'
    digest = hashlib.sha256(exe.read_bytes()).hexdigest()
    source = (ROOT / 'artifacts/debris-relaunch/inputs.bin').read_bytes()
    assert source[:8] == b'RFI6' + struct.pack('<I', 48) and len(source) == 8 + 550 * 48
    (run / 'write.bin').write_bytes(source + bytes(350 * 48))
    (run / 'read.bin').write_bytes(source[:8] + bytes(32 * 48))
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
    env.update(RF_REPLAY_PLAYER_CHECKPOINT='1', RF_REPLAY_DEV_ROOM='1',
               RF_REPLAY_LEVEL='glass_house.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
               RF_DEV_GEOMOD_CHECKPOINT_VALIDATE_AUDIT='1')
    report = dict(result='FAIL', pc_sha256=digest, cases={},
                  scope='Settled standing DEV player and destruction; exact fresh-process restart and pure malformed rejection, not full campaign persistence')

    def execute(name, recording, incoming=None, outgoing=None):
        local = dict(env)
        if incoming:
            local['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(incoming)
        if outgoing:
            local['RF_REPLAY_GEOMOD_CHECKPOINT_OUT'] = str(outgoing)
        result = subprocess.run([str(exe), '--spawn-replay', str(ROOT / 'Installed_Game'),
                                 str(recording), str(run / (name + '.ppm'))],
                                cwd=ROOT, env=local, capture_output=True, text=True, timeout=180)
        text = result.stdout + result.stderr
        (run / (name + '.log')).write_text(text)
        return result.returncode, text

    def row(text, label):
        return list(map(int, next(s for s in text.splitlines() if s.startswith(label + ' ')).split()[1:]))

    try:
        expected, restored = run / 'expected.rfcp', run / 'restored.rfcp'
        code, saved = execute('write', run / 'write.bin', outgoing=expected)
        assert code == 0, 'Capture failed; inspect write.log'
        code, loaded = execute('read', run / 'read.bin', expected, restored)
        assert code == 0, 'Restore failed; inspect read.log'
        data = expected.read_bytes()
        assert data == restored.read_bytes(), 'Full RFCP differs after fresh-process idle restart'
        assert row(saved, 'PLAYER_CHECKPOINT') == [1, 0, 1, 0, 1, len(data) - 576, 3, 1]
        assert row(loaded, 'PLAYER_CHECKPOINT') == [1, 1, 1, 0, 1, len(data) - 576, 3, 1]
        assert row(saved, 'ROCKETS')[:3] == [2, 2, 0], 'Capture must follow two real impacts'
        for text in (saved, loaded):
            assert row(text, 'PC_PLAY_BODY')[22:25] == list(struct.unpack_from('<3I', data, 64))
        assert f'PLAYER_CHECKPOINT_VALIDATE_AUDIT PASS 0 {len(data)}' in loaded
        report.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                      position=list(struct.unpack_from('<3f', data, 64)),
                      health=struct.unpack_from('<f', data, 56)[0], armor=struct.unpack_from('<f', data, 60)[0])
        subprocess.run([sys.executable, str(ROOT / 'tools/prepare_player_checkpoint_cases.py'),
                        str(expected), '--out', str(run / 'malformed')], cwd=ROOT, check=True)
        manifest = json.loads((run / 'malformed/manifest.json').read_text())
        for case in manifest['cases']:
            path = Path(case['path'])
            assert hashlib.sha256(path.read_bytes()).hexdigest() == case['sha256']
            output = run / (case['name'] + '.rfcp')
            code, text = execute(case['name'], run / 'read.bin', path, output)
            audits = [s.split() for s in text.splitlines() if s.startswith('PLAYER_CHECKPOINT_VALIDATE_AUDIT ')]
            assert code != 0 and not output.exists(), case['name'] + ' was not rejected'
            assert audits and all(len(a) == 4 and a[1] == 'PASS' and int(a[2]) != 0 and int(a[3]) == case['bytes'] for a in audits), case['name'] + ' lacks pure rejection evidence'
            assert 'PLAYER_CHECKPOINT_LOAD ' not in text and 'Completed ' not in text, case['name'] + ' published before rejection'
            report['cases'][case['name']] = dict(result='PASS', status=int(audits[-1][2]))
        assert hashlib.sha256(exe.read_bytes()).hexdigest() == digest, 'Executable changed during verification'
        report['result'] = 'PASS'
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        (run / 'report.json').write_text(json.dumps(report, indent=2))
        print(run, report['result'], flush=True)


if __name__ == '__main__':
    main()

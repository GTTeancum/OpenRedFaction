"""Bounded Xbox health/armor item class caps, difficulty and scripted grants."""
import datetime
import json
import struct
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu


def validate(guest):
    extra = guest['extra']
    f = lambda word: struct.unpack('<f', struct.pack('<I', word))[0]
    rows = [extra['rf_scene_vital_effect_test'][i:i+6] for i in (0, 6, 12, 18)]
    for row, expected in zip(rows, [(80, 0, 80, 0), (80, 0, 80, 0), (80, 0, 80, 0), (80, 150, 80, 150)]):
        if tuple(map(f, row[:4])) != expected:
            raise RuntimeError(f'Wrong health/armor/class maximum: {row}, expected {expected}')
    base = rows[0][4]
    if [row[4] for row in rows] != [base, base, base+2, base+3]:
        raise RuntimeError(f'Script medical/first aid/repair grants or full/zero-cap rejection failed: {rows}')
    if rows[1][5] != rows[0][5]+1 or rows[2][5] != rows[1][5] or rows[3][5] != rows[1][5]:
        raise RuntimeError(f'Placed medical kit must collect once after deficit: {rows}')
    if f(extra['rf_scene_pickup_vitals'][2]) != 20 or extra['rf_scene_pickups'][5] != 9789:
        raise RuntimeError('Placed medical kit must respect class health80 and restore20')
    audio = extra['rf_scene_pickup_audio']
    if audio[2:6] != [0, 0, 9789, 0] or audio[6] == 0:
        raise RuntimeError(f'Medical kit must start default powerup sound: {audio}')


def main():
    require_no_project_xemu(ROOT)
    folder = ROOT / 'artifacts/xemu' / ('vital-pickups-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag'}
    original = {n: (DISC / n).read_bytes() if (DISC / n).exists() else None for n in names}
    report = {'result': 'FAIL', 'scope': 'Xbox health/armor pickups and scripted grants'}
    try:
        for n in names:
            (DISC / n).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-item.bin').write_bytes(struct.pack('<If', 9789, 2.0))
        (DISC / 'campaign-item-effect.bin').write_bytes(struct.pack('<I', 2))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(90 * 48))
        build(folder, 'run')
        guest = run_guest(folder, 'run', ROOT / 'local/xemu-harness/pacing-base.qcow2', 90, 360,
                          snapshot=True, extra_symbols={'rf_scene_vital_effect_test': 24,
                              'rf_scene_pickups': 8, 'rf_scene_pickup_audio': 10,
                              'rf_scene_pickup_vitals': 4, 'rf_scene_nonweapon_items': 4})
        report['guest'] = guest
        validate(guest)
        report['result'] = 'PASS'
    finally:
        for n, data in original.items():
            if data is None:
                (DISC / n).unlink(missing_ok=True)
            else:
                (DISC / n).write_bytes(data)
        build(folder, 'restore')
        report['disc_restored'] = all(((DISC / n).read_bytes() if (DISC / n).exists() else None) == data for n, data in original.items())
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], flush=True)
        assert report['disc_restored']


if __name__ == '__main__':
    main()

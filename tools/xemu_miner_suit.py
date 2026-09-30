"""Bounded native Envirosuit armor refill and full-armor rejection; no images."""
import datetime
import json
import struct
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu


def validate(guest):
    extra = guest['extra']
    before, after, scripted = [extra['rf_scene_item_effect_test'][i:i+6] for i in (0, 6, 12)]
    if before[0] != before[1] or not before[5]:
        raise RuntimeError(f'Full armor must reject a nearby suit: {before}')
    expected = [before[2] + 1, before[3] + 1]
    if after[0] != after[1] or after[2:4] != expected:
        raise RuntimeError(f'Damaged armor must collect/refill once: {after}')
    if scripted[0] != scripted[1] or scripted[2:4] != expected or scripted[4] != after[4] + 1:
        raise RuntimeError(f'Script grant must refill once, then reject at full armor: {scripted}')
    audio = extra['rf_scene_pickup_audio']
    if audio[:5] != [expected[1], expected[1], 0, 0, 8614] or audio[5] in (0, 12) or audio[6] == 0:
        raise RuntimeError(f'Suit must use its authored override once: {audio}')
    if struct.pack('<2I', *audio[7:9]) != struct.pack('<2f', 5, .9):
        raise RuntimeError(f'Wrong Envirosuit sound distance/gain: {audio}')
    f = lambda bits: struct.unpack('<f', struct.pack('<I', bits))[0]
    if f(extra['rf_scene_pickup_vitals'][3]) != f(after[1]) * .75:
        raise RuntimeError('Placed suit did not restore the entire armor deficit')
    if extra['rf_scene_nonweapon_items'][1:3] != [1, 1] or extra['rf_scene_pickups'][5] != 8614:
        raise RuntimeError('Expected one placed and one scripted suit grant')


def main():
    require_no_project_xemu(ROOT)
    folder = ROOT / 'artifacts/xemu' / ('miner-suit-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag'}
    original = {n: (DISC / n).read_bytes() if (DISC / n).exists() else None for n in names}
    report = {'result': 'FAIL', 'scope': 'Xbox placed Envirosuit refill/reject and scripted grant'}
    try:
        for n in names:
            (DISC / n).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64, b'\0') + b'L8S4.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-item.bin').write_bytes(struct.pack('<If', 8614, 2.0))
        (DISC / 'campaign-item-effect.bin').write_bytes(struct.pack('<I', 1))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(90 * 48))
        build(folder, 'run')
        guest = run_guest(folder, 'run', ROOT / 'local/xemu-harness/pacing-base.qcow2', 90, 360,
                          snapshot=True, extra_symbols={'rf_scene_item_effect_test': 18,
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

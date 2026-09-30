"""Check a linked lamp Switch on stock-memory Xbox using guest state only."""
import datetime
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

from build_fragment_platform_fixture import read_entry
from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
FIXTURE = ROOT / 'artifacts/clutter-visibility-live'
SWITCH = 900101


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--save-only', action='store_true',
                        help='check save/reload of the hidden linked lamp')
    args = parser.parse_args()
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    fixture_archive = FIXTURE / 'game/levelsm.vpp'
    recipe = json.loads((FIXTURE / 'recipe.json').read_text())
    level = read_entry(fixture_archive, 'ctf06.rfl')
    if hashlib.sha256(level).hexdigest() != recipe['fixture_sha256']:
        raise RuntimeError('Prepared clutter fixture no longer matches its recipe')
    if (DISC / 'scene-fixture.vpp').exists():
        raise RuntimeError('An existing scene-fixture.vpp needs inspection before staging')
    folder = ROOT / 'artifacts/xemu' / (('clutter-visibility-save-' if args.save_only else 'clutter-visibility-') +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'scene-fixture.vpp', 'player-control.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': 'Linked clutter visibility on stock-64-MiB Xbox',
              'fixture_sha256': recipe['fixture_sha256'],
              'visual_content_verified': False}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        shutil.copyfile(fixture_archive, DISC / 'scene-fixture.vpp')
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'scene-fixture.vpp'.ljust(64, b'\0') + b'ctf06.rfl'.ljust(64, b'\0'))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(90 * 44))
        # Save hidden at frame40, show via the second setup dispatch at frame60,
        # then load at frame70; the final hidden flag must come from restoration.
        phases = (('saved', 2),) if args.save_only else (('hidden', 1), ('shown', 2))
        if args.save_only:
            (DISC / 'campaign-quick-actions.bin').write_bytes(struct.pack('<2I', 40, 70))
        for phase, activations in phases:
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<' + 'I' * activations,
                                                               *([SWITCH] * activations)))
            build(folder, phase)
            guest = run_guest(folder, phase, hdd, 90, 180, snapshot=True,
                              extra_symbols={'rf_scene_switch_runtime': 8,
                                             'rf_scene_switch_detail': 8,
                                             'rf_scene_clutter_draw': 6,
                                             'rf_scene_clutter_render_dispatch': 6})
            result[phase] = guest
        if args.save_only:
            guest = result['saved']
            detail = guest['extra']['rf_scene_switch_detail']
            dispatch = guest['extra']['rf_scene_clutter_render_dispatch']
            state = guest['checkpoint_state']
            if state[9] != 1 or state[8] != 1 or state[0] or state[3]:
                raise RuntimeError(f'Hidden lamp ordinary save/reload failed: {state}')
            if detail[0] != SWITCH or detail[2] != 1 or detail[6] != 4 or not detail[7] & 0x4000 or not dispatch[3]:
                raise RuntimeError(f'Lamp visibility did not survive reload: {detail}, {dispatch}')
        else:
            hidden = result['hidden']['extra']
            shown = result['shown']['extra']
            hd, sd = hidden['rf_scene_switch_detail'], shown['rf_scene_switch_detail']
            hc, sc = hidden['rf_scene_clutter_draw'], shown['rf_scene_clutter_draw']
            hr = hidden['rf_scene_clutter_render_dispatch']
            sr = shown['rf_scene_clutter_render_dispatch']
            if hd[0] != SWITCH or sd[0] != SWITCH or hd[2] != 1 or sd[2] != 2:
                raise RuntimeError(f'Authored Switch did not dispatch twice: {hd}, {sd}')
            if hd[6] != 4 or sd[6] != 4 or not hd[7] & 0x4000 or sd[7] & 0x4000:
                raise RuntimeError(f'Linked lamp visibility did not toggle: {hd}, {sd}')
            if hc[1] >= sc[1] or hc[2] >= sc[2]:
                raise RuntimeError(f'Hidden lamp did not reduce clutter draws: {hc}, {sc}')
            if hr[3] != 90 or not 0 < sr[3] < hr[3] or hr[5] or sr[5]:
                raise RuntimeError(f'Lamp render dispatch did not hide then show: {hr}, {sr}')
        result['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        build(folder, 'restore')
        result['disc_restored'] = all(
            ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
            for name, data in original.items())
        if not result['disc_restored']:
            result['result'] = 'FAIL'
        (folder / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
        print(folder, result['result'], flush=True)
        if not result['disc_restored']:
            raise RuntimeError('Xbox test disc flags were not restored')


if __name__ == '__main__':
    main()

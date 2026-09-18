"""Prepare a disposable authored CTF06 lamp hide/show fixture, without launching.

Uses existing process-local setup-event playback. Original archives remain
read-only. Parent runs the emitted PC recipes and inspects all three images.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import struct
from build_fragment_platform_fixture import read_entry, U, F, S
from inspect_levels import inspect as inspect_level
from inspect_clutter_records import inspect as inspect_clutter

ROOT = Path(__file__).resolve().parents[1]
TARGET = 13025
SWITCH = 900101


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'artifacts/clutter-visibility-live')
    args = parser.parse_args()
    folder = args.output_dir.resolve(); game = folder / 'game'
    game.mkdir(parents=True, exist_ok=True)
    original = read_entry(ROOT / 'Installed_Game/levelsm.vpp', 'ctf06.rfl')
    meta = inspect_level(io.BytesIO(original), dict(offset=0, size=len(original), name='ctf06.rfl'))
    section = next(s for s in meta['sections'] if s['type'] == '0x50000')
    clutter = original[section['offset']+8:section['offset']+8+section['size']]
    target = next(r for r in inspect_clutter(clutter) if r['uid'] == TARGET)
    assert target['class_name'] == b'lantern_box'
    position = struct.unpack('<3f', target['position'])
    spawn = (position[0]+3.05, 5.1, position[2])
    # Event record: enabled toggle, unlimited activation, no delay, actual
    # clutter link. Its initial dispatch leaves the original lamp visible.
    event = U(SWITCH) + S(b'Switch') + F(*position) + S(b'clutter_visibility_toggle')
    event += bytes([0]) + F(0) + bytes([1, 0]) + U(0, 0) + F(0, 0)
    event += S(b'') + S(b'') + U(1, TARGET) + bytes([255]*4)
    data = bytearray(original[:meta['sections'][0]['offset']]); offsets = {}; replaced = False
    for section in meta['sections']:
        kind = int(section['type'], 16)
        payload = original[section['offset']+8:section['offset']+8+section['size']]
        if kind == 0x600:
            # CTF fixture owns its event set; no guessed live event UID and no
            # accidental other setup effects. Original clutter stays untouched.
            payload = U(1) + event; replaced = True
        elif kind == 0x70000:
            # Disk basis is forward/right/up. Face-X with level world-up.
            payload = F(*spawn, -1,0,0, 0,0,1, 0,1,0)
        elif kind == 0 and not replaced:
            offsets[0x600] = len(data); payload_event = U(1) + event
            data += U(0x600, len(payload_event)) + payload_event
            struct.pack_into('<I', data, 20, meta['declared_sections']+1); replaced = True
        offsets[kind] = len(data); data += U(kind, len(payload)) + payload
    assert replaced
    struct.pack_into('<II', data, 12, offsets[0x70000], offsets[0x1000000])
    inspect_level(io.BytesIO(data), dict(offset=0,size=len(data),name='ctf06.rfl'))
    size = 4096 + ((len(data)+2047)&~2047); archive = bytearray(size)
    struct.pack_into('<4I', archive, 0, 0x51890ace, 1, 1, size)
    archive[2048:2057] = b'ctf06.rfl'; struct.pack_into('<I', archive, 2108, len(data))
    archive[4096:4096+len(data)] = data
    destination = game/'levelsm.vpp'
    assert not destination.exists() or destination.stat().st_nlink == 1
    destination.write_bytes(archive)
    for source in [*(ROOT/'Installed_Game').glob('*.vpp'), ROOT/'Installed_Game/bluebeard.bty']:
        if source.name.lower() == 'levelsm.vpp': continue
        destination = game/source.name
        if destination.exists(): assert os.path.samefile(source,destination)
        else: os.link(source,destination)
    jobs=[]
    for name,setup in [('baseline',None),('hidden',str(SWITCH)),('shown',f'{SWITCH},{SWITCH}')]:
        path=folder/(name+'.bin');path.write_bytes(b'RFI6'+U(48)+bytes(90*48))
        env={'RF_REPLAY_LEVEL':'ctf06.rfl','RF_REPLAY_ARCHIVE':'levelsm.vpp'}
        if setup:env['RF_REPLAY_SETUP_UID']=setup
        jobs.append(dict(name=name,env=env,command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),
            '--spawn-replay',str(game),str(path),str(folder/(name+'.ppm'))],
            expected=dict(switch_activations={'baseline':0,'hidden':1,'shown':2}[name],
                          target_visible=name!='hidden')))
    report=dict(status='PREPARED_NOT_RUN',cwd=str(ROOT),jobs=jobs,
        source_sha256=hashlib.sha256(original).hexdigest(),fixture_sha256=hashlib.sha256(data).hexdigest(),
        target=dict(uid=TARGET,class_name='lantern_box',model='lanternbox.V3D',position=position,
                    original_matrix=struct.unpack('<9f',target['matrix']),switch_uid=SWITCH),
        camera=dict(spawn=spawn,forward=[-1,0,0]),
        collision_probe=dict(start=[position[0]+1,position[1],position[2]],end=[position[0]-1,position[1],position[2]],
            expected='Exact clutter ray contact baseline/shown, no target hit hidden; parent focused scene tests provide this check.'),
        scope='Inspect all three final images for the same lamp visible/hidden/visible at its original pose; compare SWITCH_DETAIL and CLUTTER render diagnostics. Aggregate counters alone do not prove target-specific collision.',
        limitations='Disposable event and camera fixture; class only has collide_weapon, so walking through is not a valid collision control. No game/build/native launch. Clear inherited RF_REPLAY_/RF_DEV_ variables before each recipe.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n')
    print(folder/'recipe.json')


if __name__ == '__main__':
    main()

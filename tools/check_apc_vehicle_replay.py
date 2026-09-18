"""Generate bounded process-local APC entry/drive/fire/exit inputs; never launch.

The current fixture starts8.5m from the host, outside APC use radius8. The
initial fifteen forward samples approach roughly1.25m before requesting use.
Actual collision/acceleration and safe exit must be verified from replay logs.
"""
import argparse
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'artifacts/apc-vehicle-replay/inputs.bin')
    parser.add_argument('--frames', type=int, default=420, choices=(220, 420),
                        help='220 captures seated primary fire;420 includes alternate request and exit')
    args = parser.parse_args(); path = args.output.resolve(); path.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for frame in range(args.frames):
        forward = float(10 <= frame < 25 or 70 <= frame < 160)
        turn = .25 if 110 <= frame < 140 else 0
        use = frame in (40, 340)
        primary = 190 <= frame < 220
        alternate = frame == 250
        rows.append(struct.pack('<5f7I', turn, 0, forward, 0, 0, 0, 0, int(use),
                                int(primary), int(alternate), 0, 0))
    path.write_bytes(b'RFI6' + struct.pack('<I', 48) + b''.join(rows))
    manifest = {
        'status': 'PREPARED_NOT_RUN', 'frames': args.frames, 'cwd': str(ROOT),
        'env': {'RF_REPLAY_LEVEL': 'ctf06.rfl', 'RF_REPLAY_ARCHIVE': 'levelsm.vpp',
                'RF_REPLAY_DEV_ROOM': '1', 'RF_REPLAY_VEHICLE': 'apc'},
        'command': [str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                    str(ROOT / 'Installed_Game'), str(path), str(path.with_suffix('.ppm'))],
        'schedule': ['approach10..24 toward-Z', 'Use40 to board', 'drive70..159; turn110..139',
                     'primary190..219'] + (['alternate250 (deferred)', 'Use340 to exit', 'neutral through419'] if args.frames == 420 else []),
        'geometry': {'player_spawn': [30, 5, -158.5], 'host_spawn': [30, 5.4, -167],
                     'host_basis': [0, 0, -1, 0, 1, 0, 1, 0, 0], 'use_radius': 8,
                     'seat': 'interface_1', 'sphere_centers_x': [-1, 1],
                     'sphere_centers_z': [-2.5, 0, 2.5],
                     'sphere_radius': struct.unpack('<f', bytes.fromhex('6bf7c43f'))[0],
                     'note': 'Actual six APC CSPH spheres. Initial approach stays outside hull; authored spring overrides affect support.'},
        'expected': [('VEHICLE entry count1, exit count1, active0, status0' if args.frames == 420 else
                      'VEHICLE entry count1, exit count0, active1, status0'),
                     'VEHICLE_DAMAGE health5000, destroyed0, alive1',
                     'Committed host position changes during controlled drive; inspect collision/support and endpoint image',
                     'APC primary accepted shots>0; reserve equals999 minus accepted shots',
                     'Alternate fire remains deferred; its input is not an expected rocket launch'],
        'limitations': 'Generated inputs are not proof of entry, shots, safe exit, visuals or audio. No checkpoint flags: current RFVC codec is Driller-only.'
    }
    path.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()

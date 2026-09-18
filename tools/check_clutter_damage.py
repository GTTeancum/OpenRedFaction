"""Prepare baseline/pistol replays against the real authored lamp; never launch.

Requires check_clutter_visibility.py's disposable game archive. Original prop
UID/model/pose remain unchanged. Parent verifies actual ray hits and damage.
"""
import argparse
import json
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture-dir', type=Path, default=ROOT/'artifacts/clutter-visibility-live')
    parser.add_argument('--output-dir', type=Path, default=ROOT/'artifacts/clutter-damage-live')
    parser.add_argument('--aim-height-offset', type=float, default=0,
                        help='Optional measured model-local target adjustment in world Y')
    args=parser.parse_args()
    fixture=args.fixture_dir.resolve();folder=args.output_dir.resolve()
    source=json.loads((fixture/'recipe.json').read_text())
    assert source['target']['uid']==13025 and (fixture/'game/levelsm.vpp').is_file()
    target=source['target']['position'];spawn=source['camera']['spawn']
    # From the actual ninety-frame visibility baseline, not a guessed standard
    # human eye height. Prefer rereading current diagnostics if available.
    body_y=4.881521701812744;eye_y=.7854025363922119
    provenance='Recorded clutter-visibility baseline PC_PLAY_BODY and PLAYER_CLASS_EYE'
    log=fixture/'baseline.log'
    if log.exists():
        rows=log.read_text(errors='replace').splitlines()
        body=list(map(int,next(row for row in rows if row.startswith('PC_PLAY_BODY ')).split()[1:]))
        eye=list(map(int,next(row for row in rows if row.startswith('PLAYER_CLASS_EYE ')).split()[1:]))
        body_y=struct.unpack('<f',struct.pack('<I',body[23]))[0]
        eye_y=struct.unpack('<f',struct.pack('<I',eye[1]))[0]
    distance=math.hypot(spawn[0]-target[0],spawn[2]-target[2])
    aim_y=target[1]+args.aim_height_offset
    assert math.isfinite(aim_y) and distance>0
    pitch=math.atan2(aim_y-(body_y+eye_y),distance)
    # Production look update is angular_speed1,dt1/60. Live fixture confirms
    # positive input aims up: negative input moved the lamp farther above the
    # reticle (screenY188 to122), rather than toward screenY240.
    look=pitch*60/12
    assert abs(look)<=1, 'Target requires a longer bounded look interval'
    folder.mkdir(parents=True,exist_ok=True)
    jobs=[]
    shot_frames=(40,72,104,136)
    for name in ('baseline','fire'):
        rows=[]
        for frame in range(180):
            rows.append(struct.pack('<5f7I',0,0,0,look if 12<=frame<24 else 0,0,
                0,0,0,int(name=='fire' and frame in shot_frames),0,0,0))
        inputs=folder/(name+'.bin');inputs.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(rows))
        jobs.append(dict(name=name,frames=180,env=dict(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp'),
            command=[str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(fixture/'game'),str(inputs),str(folder/(name+'.ppm'))]))
    report=dict(status='PREPARED_NOT_RUN',cwd=str(ROOT),jobs=jobs,target=source['target'],
        authored=dict(life=80,material='glass',flags=['collide_weapon'],pistol_damage=40,pistol_cadence_frames=30),
        aim=dict(eye=[spawn[0],body_y+eye_y,spawn[2]],target=[target[0],aim_y,target[2]],
                 pitch_radians=pitch,look_command=look,look_frames=[12,23],evidence=provenance,sign_evidence='Live negative input moved lamp from screenY188 to122; corrected positive input aims up'),
        schedule=dict(shots=list(shot_frames),settle_before_aim_frames=12,final_frame=179),
        expected=['Baseline preserves visible live lamp UID13025.',
                  'Four separated primary requests; at least two actual forty-damage hits should deplete authored life80 if the exact posed mesh is hit.',
                  'Fire run must show target-specific damage/death and disappearance/debris; shot count alone is insufficient.',
                  'Original position and model remain unchanged. Exact ray collision must resolve this lamp before the wall behind it.'],
        limitations='Aim targets authored origin with measured standing eye, not a confirmed mesh intersection. Parent must inspect exact hits/output. No hidden event, generated substitute prop, builds or game launches. Clear inherited RF_REPLAY_/RF_DEV_ variables before each command.')
    (folder/'recipe.json').write_text(json.dumps(report,indent=2)+'\n')
    print(folder/'recipe.json')


if __name__=='__main__':
    main()

"""Generate ordinary ctf06 DEV RFI6 inputs; never launch a process.

First run retreat.bin and supply its settled body position using --body. This
separates measured movement from the analytical aiming calculation. Without
--body, shot files are explicitly candidates based on x1.25, not verified aim.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
F32 = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]
DT = F32(1/60)

def pitch_for(eye, target):
    dy = target[1]-eye[1]
    horizontal = math.hypot(target[0]-eye[0], target[2]-eye[2])
    #4a0d70 uses h=1-|sin(pitch)|, then normalizes the forward vector.
    return math.asin(dy/(horizontal+abs(dy)))

def pitch_commands(start, end, count=30):
    current = F32(start)
    result = []
    for i in range(count):
        command = F32(max(-1, min(1, (end-current)/((count-i)*DT))))
        current = F32(current+F32(DT*command))
        result.append(command)
    return result, current

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--body', nargs=3, type=float)
    ap.add_argument('--retreat-frames', type=int, default=90)
    args = ap.parse_args()
    assert 1 <= args.retreat_frames <= 120
    body = args.body or [1.25, -.401361, 2.5]
    #Exact PLAYER_CLASS_EYE words from current ctf06 idle.log.
    log = (ROOT/'artifacts/authored-post-live/idle.log').read_text()
    eye_words = next(l.split()[1:] for l in log.splitlines() if l.startswith('PLAYER_CLASS_EYE '))
    offsets = struct.unpack('<6f', struct.pack('<6I', *map(int, eye_words)))
    assert offsets[0] == offsets[2] == 0
    eye = [body[0], body[1]+offsets[1], body[2]]
    targets = [[-4.75, -.9, 2.5], [-4.75, 1.0, 2.5]]
    assert abs(body[2]-2.5) < .02, 'Retreat drift requires yaw correction; do not silently assume west.'
    distances = [math.dist(body, [t[0]+.01, t[1], t[2]]) for t in targets]
    assert min(distances) > 5.25, 'Move farther from the post before firing; original blast radius is5.'
    # Sphere-plane contact reports the projected point: aim its center one
    # collision radius outside the +X post side to retain target contactY.
    center_targets = [[t[0]+.051, t[1], t[2]] for t in targets]
    first, first_pitch = pitch_commands(0, pitch_for(eye, center_targets[0]))
    second, second_pitch = pitch_commands(first_pitch, pitch_for(eye, center_targets[1]))
    folder = ROOT/'artifacts/authored-post-live';folder.mkdir(parents=True, exist_ok=True)
    cases = {}
    for name, frames, shots in [('post-idle',180,()), ('retreat',180,()), ('aim-only',300,()),
                                ('one-shot',350,(240,)), ('two-shot',550,(240,420))]:
        rows = []
        for frame in range(frames):
            retreat = name != 'post-idle' and 60 <= frame < 60+args.retreat_frames
            pitch = first[frame-190] if name not in ('post-idle','retreat') and 190 <= frame < 220 else 0
            if name == 'two-shot' and 360 <= frame < 390:
                pitch = second[frame-360]
            rows.append(struct.pack('<5f7I', 0, 0, -.8 if retreat else 0, pitch, 0,
                                    0, 0, 0, int(frame in shots), 0,
                                    int(frame in (10,20,30,40)), 0))
        data = b'RFI6'+struct.pack('<I',48)+b''.join(rows)
        (folder/(name+'.bin')).write_bytes(data)
        cases[name] = dict(frames=frames, shots=shots, sha256=hashlib.sha256(data).hexdigest())
    report = dict(level='ctf06.rfl', initial_body=[-2.75,-.4,2.5], initial_yaw='west',
                  status='measured-position aim; runtime not tested' if args.body else 'candidate; retreat endpoint must be measured before shooting',
                  settled_body_for_aim=body, initial_eye_offsets=offsets, eye=eye, targets=targets,
                  contact_center_targets=center_targets,
                  estimated_flight_frames=[math.dist(eye,t)*60/20 for t in center_targets],
                  blast_distances=distances, max_damage_at_expected_body=0,
                  pitch_parameters=[first_pitch,second_pitch],
                  geometric_elevations=[math.atan2(t[1]-eye[1],abs(t[0]-eye[0])) for t in center_targets],
                  retreat=dict(start=60,end=60+args.retreat_frames,forward_input=-.8,settle_until=190),
                  weapon=dict(slot=4,next_frames=[10,20,30,40],speed=20,collision_radius=.051,damage=400,damage_radius=5),
                  cases=cases,
                  acceptance=['Verify retreat final position and regenerate with --body before shot runs.',
                              'First impact on replaced post face149..152 nearY-.9; real generated publication and hole, not only rocket expiry.',
                              'Second impact targets retained upper post nearY1 after180frames cooldown; verify two distinct committed cuts.',
                              'PLAYER_LIFE alive and no self blast damage; floor/water and unaffected post remain collidable.'],
                  limits='No process launch, collision test or exact ballistic integration performed. Sphere aim compensates radius.051 for the +X side. Movement endpoint, authored obstructions and post-second-cut survival require runtime checks.')
    (folder/'post-recipe.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__':
    main()

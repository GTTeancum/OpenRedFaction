"""Compare opt-in player shot traces with the selected actor's body spheres.

Capture with RF_REPLAY_TRACE=1 and RF_REPLAY_TRACE_FROM=<global frame>.
SHOT_RAY records selected candidates before world obstruction/damage.
Older captures before body-segment integration selected broad boxes alone.
This analysis measures collision-shape agreement, not visible mesh coverage.
"""
import argparse
import json
import math
from pathlib import Path


def analyze(log):
    shots = []
    for line in log.splitlines():
        words = line.split()
        if not words:
            continue
        if words[0] == 'SHOT_RAY':
            shots.append(dict(frame=int(words[1]), slot=int(words[2]),
                pellet=int(words[3]), uid=int(words[4]),
                hit_fraction=float(words[5]), origin=list(map(float, words[6:9])),
                delta=list(map(float, words[9:12])), spheres=[]))
        elif words[0] == 'SHOT_SHAPE':
            shot = shots[-1]
            assert (shot['frame'], shot['uid']) == (int(words[1]), int(words[2]))
            center = list(map(float, words[4:7]))
            radius = float(words[7])
            origin, delta = shot['origin'], shot['delta']
            length2 = sum(value * value for value in delta)
            fraction = max(0, min(1, sum((center[i] - origin[i]) * delta[i]
                for i in range(3)) / length2)) if length2 else 0
            distance = math.sqrt(sum((origin[i] + fraction * delta[i] - center[i]) ** 2
                for i in range(3)))
            shot['spheres'].append(dict(index=int(words[3]), radius=radius,
                closest_distance=distance, intersects=distance <= radius))
    for shot in shots:
        shot['body_intersection'] = (any(s['intersects'] for s in shot['spheres'])
            if shot['spheres'] else None)
    return shots


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(args.log.read_text()), indent=2))

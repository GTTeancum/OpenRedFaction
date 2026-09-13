"""Inspect authored world collision flags; does not test texture opacity."""
import argparse
import collections
import hashlib
import json
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('level')
parser.add_argument('--archive', default='levels1.vpp')
args = parser.parse_args()
probe = root / 'build/pc/Release/rf_collision_probe.exe'
raw = subprocess.check_output([str(probe), '--world-locate-dump',
                              str(root / 'Installed_Game' / args.archive), args.level])
rooms, primary = struct.unpack_from('<2I', raw)
at = 32 + 4 * primary
flags = collections.Counter()
alpha = []
for room in range(rooms):
    skip, nodes, faces = struct.unpack_from('<3I', raw, at)
    at += 12 + nodes * 40
    for index in range(faces):
        corners, bits, source = struct.unpack_from('<3I', raw, at + 40)
        at += 52 + corners * 12
        if at > len(raw):
            raise ValueError('Truncated collision face dump')
        flags[bits] += 1
        if bits & 0xc0:
            alpha.append({'room': room, 'source_face': source, 'flags': bits})
print(json.dumps({'level': args.level, 'archive': args.archive,
                  'probe_sha256': hashlib.sha256(probe.read_bytes()).hexdigest(),
                  'rooms': rooms, 'face_references': sum(flags.values()),
                  'face_flags': dict(flags), 'alpha_references': len(alpha),
                  'alpha_faces': alpha,
                  'scope': 'Retained world collision flags only; movers, bitmap opacity and contact rejection excluded.'}, indent=2))

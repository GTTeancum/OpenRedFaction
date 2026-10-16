"""Read authored hostile activation and proximity; never modify game inputs.

Proximity is a candidate-selection hint, not proof of shared rooms, visibility,
runtime activation, or combat. Layout follows src/core/level.c v180 spawn read.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from build_fragment_platform_fixture import read_entry
from xemu_turret_combat import entity_rows


def activation(row):
    raw = row['raw']
    at = row['transform'] + 48

    def string():
        nonlocal at
        length = struct.unpack_from('<H', raw, at)[0]
        at += 2
        value = raw[at:at+length].decode('cp1252')
        at += length
        return value

    string()
    friendliness = struct.unpack_from('<I', raw, at+5)[0]
    at += 13
    string()
    string()
    at += 29
    names = [string() for _ in range(7)]
    at += 18
    flags = raw[at:at+17]
    creation = (2 if flags[1] else 0) | (4 if flags[15] else 0)
    return dict(uid=row['uid'], name=row['name'],
                position=struct.unpack_from('<3f', raw, row['transform']),
                friendliness=friendliness, creation_flags=creation,
                primary=names[0], secondary=names[1])


def inspect(archive):
    with archive.open('rb') as stream:
        magic, version, count, size = struct.unpack('<4I', stream.read(16))
        if magic != 0x51890ace or version != 1 or size != archive.stat().st_size:
            raise ValueError('Invalid VPP')
        stream.seek(2048)
        names = [stream.read(64)[:60].split(b'\0')[0].decode('ascii') for _ in range(count)]
    candidates = []
    for name in names:
        if not name.lower().endswith('.rfl'):
            continue
        level = read_entry(archive, name)
        digest = hashlib.sha256(level).hexdigest()
        actors = [activation(row) for row in entity_rows(level)]
        active = [a for a in actors if a['friendliness'] == 0 and not a['creation_flags'] & 2]
        for anchor in active:
            neighbors = [a for a in active if math.dist(a['position'], anchor['position']) <= 15]
            if len(neighbors) >= 3:
                candidates.append(dict(archive=archive.name, level=name,
                    level_sha256=digest,
                    authored_count=len(actors), anchor=anchor, nearby_active=neighbors))
    return candidates


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archives', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    candidates = [c for archive in args.archives for c in inspect(archive)]
    candidates.sort(key=lambda c: len(c['nearby_active']), reverse=True)
    args.output.write_text(json.dumps(candidates, indent=2)+'\n')
    print(f'Saved {len(candidates)} candidate clusters; runtime action remains unverified.')

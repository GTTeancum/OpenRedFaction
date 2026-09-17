"""Compare a settled live crater atlas with binary-derived noise and 1555 math.

Use only after transient lights expire. Differences are reported as differences,
not automatically diagnosed as corruption. This does not inspect GPU uploads.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import struct


def audit(data):
    rows = list(csv.DictReader(io.StringIO(data.decode('ascii'))))
    if not rows:
        raise ValueError('Empty atlas')
    occupied = set()
    histogram = [0] * 32
    mismatches = []
    referenced_maps = referenced_faces = texels = 0
    for index, row in enumerate(rows):
        width, height = int(row['width']), int(row['height'])
        x, y = int(row['atlas_x']), int(row['atlas_y'])
        state = int(row['seed'])
        raw = bytes.fromhex(row['packed'])
        if (int(row['map']) != index or not 1 <= width <= 64 or
                not 1 <= height <= 64 or not 0 <= state <= 0xffffffff or
                x < 0 or y < 0 or x+width > 512 or y+height > 512 or
                len(raw) != width*height*2):
            raise ValueError('Invalid map dimensions, ID, seed or bytes')
        pixels = struct.unpack('<' + 'H'*(width*height), raw)
        used = int(row['faces'])
        if used < 0:
            raise ValueError('Negative face count')
        referenced_maps += bool(used)
        referenced_faces += used
        bad = 0
        for at, pixel in enumerate(pixels):
            location = (int(row['image']), x+at % width, y+at // width)
            if location in occupied:
                raise ValueError('Overlapping atlas allocations')
            occupied.add(location)
            # Original CRT rand, new-face fill 4e5bb0, and no-brightening pack.
            state = (state*214013+2531011) & 0xffffffff
            channel = (32 + ((state >> 16) & 32767) % 64) >> 3
            expected = 0x8000 | channel << 10 | channel << 5 | channel
            bad += pixel != expected
            if used:
                histogram[(pixel >> 10) & 31] += 1
        texels += len(pixels)
        if bad:
            mismatches.append(dict(map=index, different_texels=bad, referenced_faces=used))
    count = sum(histogram)
    populated = [i for i, n in enumerate(histogram) if n]
    return dict(result='PASS' if not mismatches else 'DIFFERENT',
                atlas_sha256=hashlib.sha256(data).hexdigest(), maps=len(rows),
                texels=texels, referenced_maps=referenced_maps,
                referenced_faces=referenced_faces, referenced_chart_texels=count,
                mismatches=mismatches, referenced_red_5bit_histogram=histogram,
                referenced_red_shader_multiplier=dict(
                    minimum=2*min(populated)/31 if count else None,
                    maximum=2*max(populated)/31 if count else None,
                    mean=2*sum(i*n for i, n in enumerate(histogram))/(31*count) if count else None),
                scope='All saved chart bytes versus recovered idle noise/packing; histogram counts whole referenced charts, not visible pixels or surface area. Multiplier is normalized 5-bit sampling times world shader factor 2, before base texture. No GPU upload, filtering, image correctness or visual parity proof.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('atlas', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.atlas.read_bytes())
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())

"""Reproduce side-view crater depth defects with process-contained player input.

Nonzero exit preserves the existing strict depth gates; inspect reports and PNGs.
No host input, artificial camera placement, or production rendering changes.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
from PIL import Image
from dev_destruction_check import ROOT, recording


def inputs(side):
    base = recording('approach') + b''.join(
        struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, int(i == 520), 0, 0, 0)
        for i in range(500, 1500))
    return base + b''.join(struct.pack('<5f7I', side if i < 55 else 0, 0, 0, 0,
        -side*.35 if i < 55 else 0, 0, 0, 0, 0, 0, 0, 0) for i in range(120))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'artifacts/geomod-side-views')
    out = parser.parse_args().out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    failed = False
    for name, side in [('left', -.8), ('right', .8)]:
        path = out / (name + '.bin')
        path.write_bytes(inputs(side))
        (out/name/'report.json').unlink(missing_ok=True)
        run = subprocess.run([sys.executable, 'tools/dev_crater_depth_check.py',
            '--input', str(path), '--out', str(out/name)], cwd=ROOT)
        failed |= run.returncode != 0
        if not (out/name/'report.json').exists():
            raise RuntimeError('Depth replay did not produce a report')
        subprocess.run([sys.executable, 'tools/analyze_crater_seams.py',
            '--folder', str(out/name)], cwd=ROOT, check=True)
        subprocess.run([sys.executable, 'tools/analyze_geomod_nearer_pixels.py',
            str(out/name)], cwd=ROOT, check=True)
        for mode in ('cut', 'intact'):
            Image.open(out/name/(mode+'.ppm')).save(out/name/(mode+'.png'))
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())

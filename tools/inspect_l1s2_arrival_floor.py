"""Text-only downward-ray survey of the first L1S2 route gap.

These are static surfaces, not a clearance or safe-landing test.
"""

import struct
import subprocess
from pathlib import Path


root = Path(__file__).resolve().parents[1]
xs = [round(-22.5 + 2.5 * i, 2) for i in range(17)]
zs = [round(-112.5 + 2.5 * i, 2) for i in range(17)]
queries = [(x, 0., z) for z in zs for x in xs]
raw = subprocess.check_output([
    str(root / 'build/pc/Release/rf_collision_probe.exe'), '--world-rays',
    str(root / 'Installed_Game/levels1.vpp'), 'L1S2.rfl'],
    input=b''.join(struct.pack('<6f', *q, 0., -40., 0.) for q in queries))
assert len(raw) == 48 * len(queries)


def symbol(index):
    status, matched, fraction, x, y, z, nx, ny, nz, face, room, hits = \
        struct.unpack_from('<iI7f3I', raw, 48 * index)
    assert status == 0
    if not matched:
        return '.'
    if ny < .4:
        return '|'
    if y >= -11:
        return 'H'
    if y >= -15:
        return '='
    if y >= -22:
        return 'v'
    return '_'


print('x', xs)
print('legend H floor>=-11, = floor[-15,-11), v floor[-22,-15), _ lower, | wall, . no hit')
for row, z in enumerate(zs):
    print(f'{z:6.1f}', ''.join(symbol(row * len(xs) + col) for col in range(len(xs))))

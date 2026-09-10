"""Exercise geometry corruption in private single-level VPP fixtures."""
import json
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
levels = json.loads((root / 'artifacts/levels.json').read_text())
level = next(l for l in levels if l['file'] == 'L1S1.rfl')
archive = next(f for f in inventory['files'] if f['path'] == 'levels1.vpp')
entry = next(e for e in archive['vpp']['entries'] if e['name'] == 'L1S1.rfl')
section = next(s for s in level['sections'] if s['type'] == '0x100')
geometry = next(g for g in json.loads((root / 'artifacts/geometry.json').read_text()) if g['file'] == 'L1S1.rfl')
with (Path(inventory['root']) / 'levels1.vpp').open('rb') as stream:
    stream.seek(entry['offset'])
    payload = stream.read(entry['size'])
size = 4096 + ((len(payload) + 2047) // 2048) * 2048
fixture = bytearray(size)
struct.pack_into('<4I', fixture, 0, 0x51890ace, 1, 1, size)
struct.pack_into('<60sI', fixture, 2048, b'L1S1.rfl', len(payload))
fixture[4096:4096+len(payload)] = payload
base = 4096 + section['offset'] + 8
faces_count = base + geometry['vertices_offset'] + geometry['vertices'] * 12
first_face = faces_count + 4
portals = next(r for r in json.loads((root/'artifacts/geometry-portals-verification.json').read_text())['results'] if r['level']=='L1S1.rfl')['portals']
assert portals > 0
first_portal = base + geometry['vertices_offset'] - 4 - portals * 32
cases = [
    ('huge_portal_count', first_portal - 4, 0xffffffff),
    ('invalid_portal_first_room', first_portal, geometry['rooms']),
    ('invalid_portal_second_room', first_portal + 4, geometry['rooms']),
    ('nonfinite_portal_bounds', first_portal + 8, 0x7fc00000),
    ('huge_texture_count', base + 6, 0xffffffff),
    ('huge_face_count', faces_count, 0xffffffff),
    ('invalid_texture', first_face + 16, geometry['textures']),
    ('invalid_room', first_face + 48, geometry['rooms']),
    ('degenerate_face', first_face + 52, 2),
    ('huge_corner_count', first_face + 52, 0xffffffff),
    ('invalid_vertex', first_face + 56, geometry['vertices']),
    ('nonfinite_plane', first_face, 0x7f800000),
    ('nonfinite_position', base + geometry['vertices_offset'], 0x7fc00000),
]
out = root / 'artifacts/geometry-corruption'
out.mkdir(exist_ok=True)
probe = root / 'build/pc/Release/rf_geometry_probe.exe'
results = []
for name, offset, value in cases:
    test = bytearray(fixture)
    struct.pack_into('<I', test, offset, value)
    path = out / 'case.vpp'
    path.write_bytes(test)
    result = subprocess.run([str(probe), str(path), 'L1S1.rfl'], capture_output=True)
    if result.returncode != 1 or b'Geometry error' not in result.stderr:
        raise AssertionError((name, result.returncode, result.stdout, result.stderr))
    results.append(dict(case=name, result='REJECTED', error=result.stderr.decode().strip()))
(out / 'report.json').write_text(json.dumps(results, indent=2))
print(f'PASS: rejected {len(results)} malformed geometry fixtures')

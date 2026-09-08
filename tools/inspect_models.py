"""Walk V3C sections structurally; do not search for signatures inside payloads.

Layout leads: Open Faction formats/v3d_format.h. Original 0x51ce60,
0x53ae5f, 0x5696f0 confirm outer dispatch, materials and LOD envelopes.
Opaque LOD blobs are retained as byte ranges, not interpreted geometry.
"""
import json
import struct
from pathlib import Path

def inspect(data):
    cursor = 0
    def take(n):
        nonlocal cursor
        if n < 0 or n > len(data) - cursor: raise ValueError(f'overrun at {cursor}, size {n}')
        start = cursor; cursor += n
        return data[start:cursor]
    def u32(): return struct.unpack('<I', take(4))[0]
    def name(n): return take(n).split(b'\0', 1)[0].decode('cp1252')
    def cstring():
        start = cursor
        while take(1) != b'\0':
            if cursor - start > 256: raise ValueError('texture name too long')
        return data[start:cursor - 1].decode('cp1252')
    header = struct.unpack('<10I', take(40))
    if header[:2] != (0x5246434d, 0x40000): raise ValueError('unsupported model header')
    sections = []; meshes = 0
    while cursor < len(data):
        start = cursor; kind, declared = u32(), u32()
        section = dict(offset=start, type=hex(kind), declared=declared)
        if kind == 0:
            if declared or cursor != len(data): raise ValueError('bad end section')
            sections.append(section); break
        if kind != 0x5355424d:
            take(declared)
        else:
            meshes += 1
            section['name'] = name(24); section['group'] = name(24)
            version, lods = u32(), u32()
            if not 7 <= version <= 0x7fffffff or not 1 <= lods <= 3: raise ValueError(f'bad submesh version/LODs at {start}')
            take(lods * 4 + 40)
            section['lods'] = []
            for _ in range(lods):
                flags, unknown = u32(), u32()
                batches = struct.unpack('<H', take(2))[0]
                size = u32(); blob = cursor; take(size)
                after_blob = u32(); take(batches * 18)
                props, textures = u32(), u32()
                texture_names = []
                for _ in range(textures):
                    slot = take(1)[0]; texture_names.append(dict(slot=slot, name=cstring()))
                section['lods'].append(dict(flags=flags, unknown=unknown, batches=batches,
                                           data_offset=blob, data_bytes=size, after_blob=after_blob,
                                           props=props, textures=texture_names))
            materials = u32(); take(materials * 84)
            groups = u32(); take(groups * 28)
            section['materials'] = materials
        section['bytes'] = cursor - start
        sections.append(section)
    if not sections or sections[-1]['type'] != '0x0': raise ValueError('no end section')
    if meshes != header[2]: raise ValueError('submesh count mismatch')
    return dict(bytes=len(data), submeshes=meshes, sections=sections)

def main():
    root = Path(__file__).resolve().parents[1]
    inventory = json.loads((root / 'artifacts/inventory.json').read_text())
    reports, failures = [], []
    for archive in inventory['files']:
        for entry in archive.get('vpp', {}).get('entries', []):
            if not entry['name'].lower().endswith('.v3c'): continue
            with (root / 'Installed_Game' / archive['path']).open('rb') as f:
                f.seek(entry['offset']); data = f.read(entry['size'])
            try:
                result = inspect(data)
                reports.append(dict(archive=archive['path'], model=entry['name'], **result))
            except (ValueError, struct.error) as error:
                failures.append(dict(archive=archive['path'], model=entry['name'], error=str(error)))
    output = dict(models=reports, failures=failures, scope='Structural envelope only; opaque LOD blobs')
    (root / 'artifacts/model-structure.json').write_text(json.dumps(output, indent=2))
    print(f'{len(reports)} models parsed, {len(failures)} failures')
    print(failures[:5])
    if failures: raise SystemExit(1)

if __name__ == '__main__': main()

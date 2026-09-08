"""Inspect v180 static geometry; byte layout leads are documented in PROVENANCE.md."""
import json
import struct
from pathlib import Path

def inspect(data):
    cursor = 6
    def take(n):
        nonlocal cursor
        if n < 0 or cursor + n > len(data):
            raise ValueError(f'Geometry ends at {len(data)}, requested {n} at {cursor}')
        value = data[cursor:cursor+n]
        cursor += n
        return value
    def number():
        return struct.unpack('<I', take(4))[0]
    def string():
        return take(struct.unpack('<H', take(2))[0]).decode('cp1252')
    textures = [string() for _ in range(number())]
    take(number() * 12)
    rooms = number()
    for _ in range(rooms):
        raw = take(40)
        string()
        if raw[32]:
            take(8); string(); take(37)
        if raw[33]:
            take(4)
    links = number()
    for _ in range(links):
        number(); take(number() * 4)
    take(number() * 32)
    vertices = number()
    vertices_offset = cursor
    take(vertices * 12)
    faces = number()
    corners = 0
    max_face = 0
    for _ in range(faces):
        face = take(56)
        texture, lightmap = struct.unpack_from('<II', face, 16)
        room, count = struct.unpack_from('<II', face, 48)
        assert texture == 0xffffffff or texture < len(textures)
        assert room < rooms and count >= 3
        stride = 12 if lightmap == 0xffffffff else 20
        for _ in range(count):
            vertex = take(stride)
            assert struct.unpack_from('<I', vertex)[0] < vertices
        max_face = max(count, max_face)
        corners += count
    mappings = number()
    take(mappings * 96)
    tail_word = number()
    tail_bytes = len(data) - cursor
    return dict(textures=len(textures), rooms=rooms, vertices=vertices, faces=faces,
                corners=corners, max_face=max_face, mappings=mappings, bytes=len(data),
                vertices_offset=vertices_offset, tail_word=tail_word, tail_bytes=tail_bytes)

def main():
    root = Path(__file__).resolve().parents[1]
    inventory = json.loads((root / 'artifacts/inventory.json').read_text())
    levels = json.loads((root / 'artifacts/levels.json').read_text())
    reports = []
    for level in levels:
        section = next(s for s in level['sections'] if s['type'] == '0x100')
        archive = next(f for f in inventory['files'] if f['path'] == level['archive'])
        entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
        with (Path(inventory['root']) / level['archive']).open('rb') as stream:
            stream.seek(entry['offset'] + section['offset'] + 8)
            data = stream.read(section['size'])
        report = inspect(data)
        report.update(file=level['file'], archive=level['archive'])
        reports.append(report)
    (root / 'artifacts/geometry.json').write_text(json.dumps(reports, indent=2))
    print(f'Parsed known geometry fields in {len(reports)} payloads; unrecognized tails recorded')
    print(next(r for r in reports if r['file'] == 'L1S1.rfl'))

if __name__ == '__main__':
    main()

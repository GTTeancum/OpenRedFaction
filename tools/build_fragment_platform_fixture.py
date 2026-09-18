"""Build a disposable CTF06 platform fixture; never modify installed game inputs.

Generated archives contain local game assets and must remain untracked. Other
VPPs are read-only hardlinks, not duplicate payloads. The platform uses the normal
RFL mover loader/render/collision path. This script does not yet animate it.
"""
import hashlib
import io
import json
import os
from pathlib import Path
import struct

from inspect_levels import inspect as inspect_level
from inspect_geometry import inspect as inspect_geometry

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/fragment-platform'
GAME = OUT / 'game'
U = lambda *v: struct.pack('<' + 'I' * len(v), *v)
F = lambda *v: struct.pack('<' + 'f' * len(v), *v)
S = lambda v: struct.pack('<H', len(v)) + v


def read_entry(path, name):
    with path.open('rb') as f:
        magic, version, count, size = struct.unpack('<4I', f.read(16))
        assert magic == 0x51890ace and version == 1 and size == path.stat().st_size
        cursor = 2048 + ((count * 64 + 2047) & ~2047)
        f.seek(2048)
        table = f.read(count * 64)
        for index in range(count):
            entry = table[index * 64:(index + 1) * 64]
            length, = struct.unpack_from('<I', entry, 60)
            if entry[:60].split(b'\0')[0].decode('ascii').lower() == name.lower():
                f.seek(cursor)
                data = f.read(length)
                assert len(data) == length
                return data
            cursor += (length + 2047) & ~2047
    raise ValueError(name)


def platform_geometry(texture):
    vertices = [(-1.5,-.1,-1.5),(1.5,-.1,-1.5),(1.5,.1,-1.5),(-1.5,.1,-1.5),
                (-1.5,-.1,1.5),(1.5,-.1,1.5),(1.5,.1,1.5),(-1.5,.1,1.5)]
    faces = [(1,2,6,5),(0,4,7,3),(3,7,6,2),(0,1,5,4),(4,5,6,7),(0,3,2,1)]
    planes = [(1,0,0,-1.5),(-1,0,0,-1.5),(0,1,0,-.1),(0,-1,0,-.1),(0,0,1,-1.5),(0,0,-1,-1.5)]
    data = bytes(6) + U(1) + S(texture) + U(0,0,0,0,8)
    data += b''.join(F(*v) for v in vertices) + U(6)
    for indices, plane in zip(faces, planes):
        header = bytearray(56)
        struct.pack_into('<4f', header, 0, *plane)
        struct.pack_into('<II', header, 16, 0, 0xffffffff)
        struct.pack_into('<I', header, 28, 0xffffffff)
        struct.pack_into('<II', header, 48, 0xffffffff, 4)
        data += header + b''.join(U(i) + F(*uv) for i, uv in zip(indices, ((0,0),(0,1),(1,1),(1,0))))
    data += U(0,0)
    check = inspect_geometry(data, allow_unowned=True)
    assert check['faces'] == 6 and check['vertices'] == 8 and check['tail_bytes'] == 0
    return data


def main():
    GAME.mkdir(parents=True, exist_ok=True)
    original = read_entry(ROOT / 'Installed_Game/levelsm.vpp', 'ctf06.rfl')
    meta = inspect_level(io.BytesIO(original), dict(offset=0, size=len(original), name='ctf06.rfl'))
    section = next(s for s in meta['sections'] if s['type'] == '0x100')
    geometry = original[section['offset'] + 8:section['offset'] + 8 + section['size']]
    # Reuse a texture already present in this level; no extra asset is downloaded.
    count, = struct.unpack_from('<I', geometry, 6)
    assert count
    names = []; cursor = 10
    for _ in range(count):
        length, = struct.unpack_from('<H', geometry, cursor); cursor += 2
        names.append(geometry[cursor:cursor+length]); cursor += length
    texture = next((n for n in names if b'metal' in n.lower()), names[0])
    # Disk orientation is forward/right/up; runtime basis is right/up/forward.
    mover = U(1, 900001) + F(9.449,.55,2.5) + F(0,0,1,1,0,0,0,1,0)
    mover += platform_geometry(texture) + U(0,0,0)
    data = bytearray(original[:meta['sections'][0]['offset']])
    offsets = {}; replaced = False
    for section in meta['sections']:
        kind = int(section['type'], 16)
        payload = original[section['offset']+8:section['offset']+8+section['size']]
        if kind == 0x2000:
            assert struct.unpack_from('<I', payload)[0] == 0
            payload = mover; replaced = True
        if kind == 0 and not replaced:
            offsets[0x2000] = len(data)
            data += U(0x2000, len(mover)) + mover
            struct.pack_into('<I', data, 20, meta['declared_sections'] + 1)
            replaced = True
        offsets[kind] = len(data)
        data += U(kind, len(payload)) + payload
    assert replaced
    struct.pack_into('<II', data, 12, offsets[0x70000], offsets[0x1000000])
    inspect_level(io.BytesIO(data), dict(offset=0, size=len(data), name='ctf06.rfl'))
    length = 4096 + ((len(data)+2047) & ~2047)
    archive = bytearray(length)
    struct.pack_into('<4I', archive, 0, 0x51890ace, 1, 1, length)
    archive[2048:2057] = b'ctf06.rfl'
    struct.pack_into('<I', archive, 2108, len(data))
    archive[4096:4096+len(data)] = data
    target = GAME / 'levelsm.vpp'
    assert not target.exists() or target.stat().st_nlink == 1, 'Refusing to overwrite a linked original archive'
    target.write_bytes(archive)
    links = 0
    for source in [*(ROOT / 'Installed_Game').glob('*.vpp'), ROOT / 'Installed_Game/bluebeard.bty']:
        if source.name.lower() == 'levelsm.vpp': continue
        destination = GAME / source.name
        if destination.exists(): assert os.path.samefile(source, destination)
        else: os.link(source, destination)
        links += 1
    report = dict(source_sha256=hashlib.sha256(original).hexdigest(), fixture_sha256=hashlib.sha256(data).hexdigest(),
                  mover_uid=900001, center=[9.449,.55,2.5], half_extent=[1.5,.1,1.5], texture=texture.decode('ascii'),
                  archive_bytes=length, linked_inputs=links,
                  scope='Explicit static developer platform through normal mover ownership; motion and rubble acceptance remain pending.')
    (OUT / 'build.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__': main()

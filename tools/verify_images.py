"""Check installed TGA variants and Live Mines against Pillow; exercise bad input."""
import io
import json
import struct
import subprocess
from pathlib import Path
from PIL import Image


def main():
    root = Path(__file__).resolve().parents[1]
    inventory = json.loads((root / 'artifacts/inventory.json').read_text())
    levels = json.loads((root / 'artifacts/levels.json').read_text())
    level = next(l for l in levels if l['file'] == 'L1S1.rfl')
    section = next(s for s in level['sections'] if s['type'] == '0x100')
    archive = next(a for a in inventory['files'] if a['path'] == level['archive'])
    entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
    with (root / 'Installed_Game' / archive['path']).open('rb') as stream:
        stream.seek(entry['offset'] + section['offset'] + 8)
        geometry = stream.read(section['size'])
    names, cursor = [], 10
    for _ in range(struct.unpack_from('<I', geometry, 6)[0]):
        length = struct.unpack_from('<H', geometry, cursor)[0]; cursor += 2
        names.append(geometry[cursor:cursor+length].decode('cp1252')); cursor += length
    output = root / 'artifacts/image-tests'
    output.mkdir(exist_ok=True)
    probe = root / 'build/pc/Release/rf_image_probe.exe'
    raw = output / 'decoded.rgba'
    seen, verified, materials, unsupported = set(), [], [], []
    for archive in inventory['files']:
        entries = [e for e in archive.get('vpp', {}).get('entries', []) if e['name'].lower().endswith('.tga')]
        if not entries:
            continue
        path = root / 'Installed_Game' / archive['path']
        with path.open('rb') as stream:
            for entry in entries:
                stream.seek(entry['offset']); header = stream.read(18)
                variant = (header[1], header[2], header[16], header[17])
                material = entry['name'].lower() in [n.lower() for n in names]
                width, height = struct.unpack_from('<HH', header, 12)
                if material:
                    materials.append(dict(name=entry['name'], archive=archive['path'],
                                          width=width, height=height, rgba_bytes=width*height*4))
                if header[1] or header[2] not in (2, 10):
                    unsupported.append(dict(name=entry['name'], archive=archive['path'], variant=variant))
                    continue
                if not (material or variant not in seen or header[2] == 10):
                    continue
                seen.add(variant)
                stream.seek(entry['offset']); data = stream.read(entry['size'])
                decoded = Image.open(io.BytesIO(data)).convert('RGBA')
                # The decoder explicitly makes unspecified attribute bits opaque.
                if header[17] & 15 == 0:
                    decoded.putalpha(255)
                result = subprocess.run([str(probe), str(path), entry['name'], str(raw), str(width*height*4)], capture_output=True, text=True)
                assert result.returncode == 0, (entry, result.stdout, result.stderr)
                assert raw.read_bytes() == decoded.tobytes(), entry['name']
                too_small = subprocess.run([str(probe), str(path), entry['name'], str(raw), str(width*height*4-1)], capture_output=True, text=True)
                assert too_small.returncode == 1 and too_small.stdout.strip() == '-4', entry['name']
                verified.append(dict(name=entry['name'], archive=archive['path'], variant=variant))
    # One-entry VPP fixtures test all origins, mixed RLE packets, and rejection.
    def fixture(data):
        size = 4096 + ((len(data)+2047)//2048)*2048
        content = bytearray(size)
        struct.pack_into('<IIII', content, 0, 0x51890ace, 1, 1, size)
        content[2048:2056] = b'test.tga'
        struct.pack_into('<I', content, 2108, len(data))
        content[4096:4096+len(data)] = data
        target = output / 'fixture.vpp'; target.write_bytes(content)
        return subprocess.run([str(probe), str(target), 'test.tga', str(raw), '1024'], capture_output=True, text=True)
    fixture_count = 0
    for origin in (0, 16, 32, 48):
        for kind in (2, 10):
            h = bytearray(18); h[2] = kind; h[16] = 24; h[17] = origin
            struct.pack_into('<HH', h, 12, 2, 2)
            pixels = bytes([0,0,255, 0,0,255, 0,255,0, 255,0,0])
            payload = pixels if kind == 2 else bytes([129])+pixels[:3]+bytes([1])+pixels[6:]
            data = h + payload
            assert fixture(data).returncode == 0
            assert raw.read_bytes() == Image.open(io.BytesIO(data)).convert('RGBA').tobytes()
            fixture_count += 1
            assert fixture(data[:-1]).returncode == 1
            fixture_count += 1
    h[2] = 10
    assert fixture(h + bytes([132,0,0,0])).stdout.strip() == '-2'
    fixture_count += 1
    report = dict(verified=verified, synthetic_cases=fixture_count, materials=materials,
                  material_rgba_bytes=sum(m['rgba_bytes'] for m in materials),
                  unresolved=[n for n in names if n.lower() not in [m['name'].lower() for m in materials]],
                  unsupported=unsupported)
    (output / 'report.json').write_text(json.dumps(report, indent=2))
    print(f'PASS: {len(verified)} installed textures, {fixture_count} synthetic cases; {len(materials)} Live Mines materials')
    print(f'RGBA bytes: {report["material_rgba_bytes"]}; unresolved: {report["unresolved"]}; unsupported images: {len(unsupported)}')


if __name__ == '__main__':
    main()

"""Check original v180 lightmap image boundaries and decoded allocation sizes.

Layout lead: Open Faction CLightmaps.cpp and rfl_format.h; no engine semantics
or original lightmap blend equation are inferred from successful parsing.
"""
import json
import struct
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[1]
    inventory = json.loads((root/'artifacts/inventory.json').read_text())
    levels = json.loads((root/'artifacts/levels.json').read_text())
    reports = []
    for level in levels:
        archive = next(a for a in inventory['files'] if a['path'] == level['archive'])
        entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
        section = next(s for s in level['sections'] if s['type'] == '0x1200')
        with (Path(inventory['root'])/archive['path']).open('rb') as stream:
            stream.seek(entry['offset']+section['offset']+8)
            data = stream.read(section['size'])
        assert len(data) >= 4
        count, = struct.unpack_from('<I', data)
        cursor, images = 4, []
        for _ in range(count):
            assert cursor+8 <= len(data), level['file']
            width, height = struct.unpack_from('<II', data, cursor); cursor += 8
            assert width and height and width <= 4096 and height <= 4096
            size = width*height*3
            assert cursor+size <= len(data), level['file']
            images.append(dict(width=width, height=height, rgb_offset=cursor, rgb_bytes=size))
            cursor += size
        assert cursor == len(data), (level['file'], cursor, len(data))
        reports.append(dict(file=level['file'], archive=archive['path'], count=count,
                            section_bytes=len(data), rgba_bytes=sum(i['width']*i['height']*4 for i in images), images=images))
    (root/'artifacts/lightmaps.json').write_text(json.dumps(reports, indent=2))
    live = next(r for r in reports if r['file'] == 'L1S1.rfl')
    print(f'PASS: exact lightmap section coverage for {len(reports)} levels')
    print({k:v for k,v in live.items() if k != 'images'})
    print('Largest RGBA expansion:', max(r['rgba_bytes'] for r in reports))


if __name__ == '__main__':
    main()

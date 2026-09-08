"""Inspect RFL section boundaries directly in VPPs, without extracting game data."""
import argparse
import json
import struct
from pathlib import Path

TYPES = {
    0: 'end', 0x100: 'static_geometry', 0x200: 'geo_regions', 0x300: 'lights',
    0x400: 'cutscene_cameras', 0x500: 'ambient_sounds', 0x600: 'events',
    0x700: 'mp_respawns', 0x900: 'level_properties', 0xa00: 'particle_emitters',
    0xb00: 'gas_regions', 0xc00: 'room_effects', 0xe00: 'bolt_emitters',
    0xf00: 'targets', 0x1000: 'decals', 0x1100: 'push_regions',
    0x1200: 'lightmaps', 0x2000: 'movers', 0x3000: 'moving_groups',
    0x5000: 'cutscene_path_nodes', 0x8000: 'eax_effects', 0x20000: 'nav_points',
    0x30000: 'entities', 0x40000: 'items', 0x50000: 'clutters',
    0x60000: 'triggers', 0x70000: 'player_start', 0x1000000: 'level_info',
    0x2000000: 'brushes', 0x3000000: 'groups',
}


def inspect(stream, entry):
    base, length = entry['offset'], entry['size']
    cursor = 0

    def read(size):
        nonlocal cursor
        if size < 0 or cursor + size > length:
            raise ValueError(f'{entry["name"]}: read exceeds entry at {cursor}')
        stream.seek(base + cursor)
        data = stream.read(size)
        if len(data) != size:
            raise ValueError('Truncated archive')
        cursor += size
        return data

    def string():
        count, = struct.unpack('<H', read(2))
        return read(count).decode('cp1252')

    magic, version, timestamp, player, info, count, unknown = struct.unpack('<7I', read(28))
    if magic != 0xd4bada55:
        raise ValueError(f'{entry["name"]}: invalid RFL signature')
    name, mod = string(), string()
    sections = []
    while cursor < length:
        offset = cursor
        kind, size = struct.unpack('<2I', read(8))
        if cursor + size > length:
            raise ValueError(f'{entry["name"]}: section outside file')
        sections.append(dict(type=hex(kind), kind=TYPES.get(kind, 'unknown'), offset=offset, size=size))
        cursor += size
        if kind == 0:
            break
    if not sections or sections[-1]['type'] != '0x0':
        raise ValueError(f'{entry["name"]}: no end section')
    offsets = {s['offset']: s['kind'] for s in sections}
    return dict(file=entry['name'], bytes=length, name=name, mod=mod, version=version,
                timestamp=timestamp, declared_sections=count, observed_sections=len(sections),
                trailing_bytes=length-cursor, player_start_offset=player, level_info_offset=info,
                player_offset_matches=offsets.get(player) == 'player_start',
                info_offset_matches=offsets.get(info) == 'level_info', unknown_header_word=unknown,
                sections=sections)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inventory', type=Path, default=Path('artifacts/inventory.json'))
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
    levels = []
    for record in inventory['files']:
        if 'vpp' not in record:
            continue
        with (Path(inventory['root']) / record['path']).open('rb') as stream:
            for entry in record['vpp']['entries']:
                if entry['name'].lower().endswith('.rfl'):
                    level = inspect(stream, entry)
                    level['archive'] = record['path']
                    levels.append(level)
    Path('artifacts/levels.json').write_text(json.dumps(levels, indent=2), encoding='utf-8')
    print(f'Inspected {len(levels)} level files; see artifacts/levels.json')
    first = next(level for level in levels if level['file'].lower() == 'l1s1.rfl')
    print(json.dumps(first, indent=2))


if __name__ == '__main__':
    main()

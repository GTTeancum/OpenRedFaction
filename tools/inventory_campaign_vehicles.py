"""List authored single-player vehicle instances directly from installed RFLs.

This is a data inventory only: no game executable or desktop input is used.
"""
import json
import math
from pathlib import Path
import re
import struct

from inspect_levels import inspect


ROOT = Path(__file__).resolve().parents[1]
VEHICLES = {'Driller01', 'APC', 'Jeep01', 'sub', 'Fighter01'}


def records(data):
    cursor = 0

    def take(size):
        nonlocal cursor
        if size < 0 or size > len(data) - cursor:
            raise ValueError('Entity section is truncated')
        value = data[cursor:cursor + size]
        cursor += size
        return value

    def string():
        size, = struct.unpack('<H', take(2))
        return take(size).decode('cp1252')

    count, = struct.unpack('<I', take(4))
    for _ in range(count):
        uid, = struct.unpack('<i', take(4))
        class_name = string()
        transform = take(48)
        script_name = string()
        take(13)  # authored relationships
        string()
        string()
        take(29)
        for _ in range(7):
            string()
        take(18)
        flags = take(17)
        if flags[-1] not in (0, 1):
            raise ValueError('Invalid entity optional-field marker')
        if flags[-1]:
            take(4)
        string()
        string()
        if class_name in VEHICLES:
            yield {'uid': uid, 'class': class_name, 'script': script_name,
                   'position': struct.unpack('<3f', transform[:12]),
                   'orientation': struct.unpack('<9f', transform[24:48] + transform[12:24])}
    if cursor != len(data):
        raise ValueError('Entity section has trailing bytes')


def main():
    inventory = json.loads((ROOT / 'artifacts/inventory.json').read_text())
    found = []
    for archive in inventory['files']:
        for entry in archive.get('vpp', {}).get('entries', []):
            if not re.fullmatch(r'L\d+S\d+\.rfl', entry['name'], re.I):
                continue
            path = Path(inventory['root']) / archive['path']
            with path.open('rb') as stream:
                level = inspect(stream, entry)
                section = next((part for part in level['sections']
                                if part['type'] == '0x30000'), None)
                if section is None:
                    continue
                stream.seek(entry['offset'] + section['offset'] + 8)
                data = stream.read(section['size'])
                stream.seek(entry['offset'] + level['player_start_offset'] + 8)
                start = struct.unpack('<3f', stream.read(12))
            for record in records(data):
                record['distance_to_start'] = math.dist(record['position'], start)
                found.append({'archive': archive['path'], 'level': entry['name'],
                              **record})
    print(json.dumps(found, indent=2))


if __name__ == '__main__':
    main()

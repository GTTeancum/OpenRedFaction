"""Audit version180 RFL navigation records against the463d50 read sequence.

This is an analysis reader, not the shared runtime loader. Preserve unknown
words and raw float bytes; names for unidentified fields use node offsets.
"""
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def inspect(data, version=180):
    if version != 180:
        raise ValueError('Only the installed version180 layout is audited')
    cursor = 0

    def take(size):
        nonlocal cursor
        if size < 0 or cursor + size > len(data):
            raise ValueError(f'Truncated navigation section at {cursor}')
        result = data[cursor:cursor + size]
        cursor += size
        return result

    def word():
        return struct.unpack('<I', take(4))[0]

    count = word()
    if count > (len(data) - 4) // 42:
        raise ValueError('Navigation count exceeds minimum record storage')
    nodes = []
    for _ in range(count):
        node = dict(uid=word(), discarded_enabled=take(1)[0])
        node['height'] = take(4)
        node['position'] = take(12)
        node['radius'] = take(4)
        node['word_040'] = word()
        node['oriented'] = int(take(1)[0] != 0)
        node['orientation'] = take(36) if node['oriented'] else None
        node['discarded_flags'] = take(3)
        node['word_024'] = take(4)
        tag_count = word()
        if tag_count > (len(data) - cursor) // 4:
            raise ValueError('Navigation tag count exceeds section storage')
        node['tags'] = [word() for _ in range(tag_count)]
        nodes.append(node)
    for node in nodes:
        raw = [word() for _ in range(take(1)[0])]
        node['raw_neighbors'] = raw
        # Original466220 retains first occurrences; invalid signed indices skip.
        node['neighbors'] = list(dict.fromkeys(i for i in raw if i < count))
    if cursor != len(data):
        raise ValueError(f'Unconsumed navigation bytes: {len(data) - cursor}')
    return nodes


def sections():
    inventory = json.loads((ROOT / 'artifacts/inventory.json').read_text())
    levels = json.loads((ROOT / 'artifacts/levels.json').read_text())
    for level in levels:
        archive = next(a for a in inventory['files'] if a['path'] == level['archive'])
        entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
        for section in level['sections']:
            if section['type'] != '0x20000':
                continue
            with (ROOT / 'Installed_Game' / level['archive']).open('rb') as stream:
                stream.seek(entry['offset'] + section['offset'] + 8)
                data = stream.read(section['size'])
            yield level, data


def main():
    rows = []
    for level, data in sections():
        nodes = inspect(data, level['version'])
        rows.append(dict(file=level['file'], bytes=len(data), nodes=len(nodes),
                         oriented=sum(n['oriented'] for n in nodes),
                         tags=sum(len(n['tags']) for n in nodes),
                         raw_edges=sum(len(n['raw_neighbors']) for n in nodes),
                         edges=sum(len(n['neighbors']) for n in nodes)))
    report = dict(result='PASS', sections=len(rows),
                  nodes=sum(r['nodes'] for r in rows), levels=rows)
    (ROOT / 'artifacts/navigation-records.json').write_text(json.dumps(report, indent=2))
    print({k: v for k, v in report.items() if k != 'levels'})
    print(next(r for r in rows if r['file'].lower() == 'l1s1.rfl'))


if __name__ == '__main__':
    main()

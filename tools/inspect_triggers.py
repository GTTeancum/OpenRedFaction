"""Bounded v180 trigger inventory from original loader 465510 read order."""
import json
import math
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def inspect(data):
    at = 0

    def take(size):
        nonlocal at
        if size < 0 or size > len(data)-at:
            raise ValueError((at, size, len(data)))
        value = data[at:at+size]
        at += size
        return value

    def number():
        return struct.unpack('<I', take(4))[0]

    def byte():
        return take(1)[0]

    def string():
        return take(struct.unpack('<H', take(2))[0]).decode('cp1252')

    def floats(count):
        values = list(struct.unpack('<'+'f'*count, take(count*4)))
        if not all(math.isfinite(value) for value in values):
            raise ValueError(('nonfinite', at))
        return values

    records = []
    for _ in range(number()):
        start = at
        uid, name, enabled, shape = number(), string(), byte(), number()
        timing = floats(1)[0]  # v134+: float read, then original converts to int.
        unknown = number()
        flag0, script, flag1, value = byte(), string(), byte(), byte()
        flags = [flag0, flag1, byte(), byte(), byte()]
        position = floats(3)
        if shape == 0:
            geometry = dict(radius=floats(1)[0])
        elif shape == 1:
            geometry = dict(orientation_disk=floats(9), dimensions_disk=floats(3), flag=byte())
        else:
            raise ValueError(('unsupported shape', shape, start))
        fields = [number(), number(), number()]
        tail_flag, values, tail_word = byte(), floats(2), number()
        count = number()
        if count > (len(data)-at)//4:
            raise ValueError(('link count', count, at))
        links = [number() for _ in range(count)]
        records.append(dict(uid=uid, name=name, offset=start, bytes=at-start,
                            enabled_byte=enabled, shape=shape, timing=timing,
                            unknown_word=unknown, flags=flags, script=script,
                            value_byte=value, position=position, **geometry,
                            fields=fields, tail_flag=tail_flag, values=values,
                            tail_word=tail_word, links=links))
    if at != len(data):
        raise ValueError(('trailing bytes', at, len(data)))
    return records


def main():
    inventory = json.loads((root/'artifacts/inventory.json').read_text())
    groups = json.loads((root/'artifacts/moving-groups.json').read_text())['results']
    results = []
    for level in json.loads((root/'artifacts/levels.json').read_text()):
        section = next((s for s in level['sections'] if s['type']=='0x60000'), None)
        if not section:
            continue
        if level['version'] != 180:
            raise ValueError(('unsupported version', level['file'], level['version']))
        archive = next(a for a in inventory['files'] if a['path']==level['archive'])
        entry = next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
        with (root/'Installed_Game'/level['archive']).open('rb') as stream:
            stream.seek(entry['offset']+section['offset']+8)
            data = stream.read(section['size'])
        try:
            records = inspect(data)
        except Exception as error:
            raise ValueError(level['file']) from error
        group_level = next((g for g in groups if g['file']==level['file'] and g['archive']==level['archive']), None)
        key_index = {}
        if group_level:
            for group in group_level['records']:
                for index, key in enumerate(group['keys']):
                    key_index.setdefault(key['uid'], []).append(dict(group=group['name'], key_index=index,
                                                                     mover_uids=group['ids2']))
        for record in records:
            record['key_link_matches'] = [dict(uid=uid, matches=key_index[uid]) for uid in record['links'] if uid in key_index]
            record['other_links'] = [uid for uid in record['links'] if uid not in key_index]
        results.append(dict(file=level['file'], archive=level['archive'], records=records))
    report = dict(result='PASS', levels=len(results),
                  triggers=sum(len(level['records']) for level in results),
                  links=sum(len(record['links']) for level in results for record in level['records']),
                  scope='Python v180 layout inventory from 465510 read sequence; exact section exhaustion and finite floats. '
                        'Field names provisional; not original parser execution, C reader, runtime handle resolution or trigger behavior.',
                  results=results)
    (root/'artifacts/triggers.json').write_text(json.dumps(report, indent=2))
    print({key: value for key, value in report.items() if key != 'results'})
    live = next(level for level in results if level['file']=='L1S1.rfl')
    print([dict(uid=record['uid'], links=record['links'], key_link_matches=record['key_link_matches'],
                other_links=record['other_links']) for record in live['records'] if record['name']=='Trigger Door'])


if __name__ == '__main__':
    main()

"""Read installed RFLs in place to locate authored NPC hauling encounters.

No game execution, archive extraction, fixture edits or output files. Distances
are Euclidean hints, not navigation/visibility or successful hauling claims.
"""
import argparse
import json
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from build_fragment_platform_fixture import read_entry
from inspect_levels import inspect as inspect_level
from inspect_events import inspect as inspect_events
from inspect_clutter_records import inspect as inspect_clutter
from check_ai_projectile_ordinary import records as entity_records


def entries(stream):
    magic, version, count, size = struct.unpack('<4I', stream.read(16))
    if magic != 0x51890ace or version != 1:
        raise ValueError('Unsupported VPP header')
    stream.seek(0, 2)
    if stream.tell() != size or count * 64 > size - 2048:
        raise ValueError('Invalid VPP size')
    stream.seek(2048)
    table = stream.read(count * 64)
    offset = 2048 + ((count * 64 + 2047) & ~2047)
    for index in range(count):
        row = table[index * 64:(index + 1) * 64]
        length = struct.unpack_from('<I', row, 60)[0]
        if offset + length > size:
            raise ValueError('VPP entry outside archive')
        yield dict(name=row[:60].split(b'\0')[0].decode('cp1252'),
                   offset=offset, size=length)
        offset += (length + 2047) & ~2047


def collectable_classes(game):
    text = read_entry(game / 'tables.vpp', 'clutter.tbl').decode('cp1252')
    text = re.sub(r'"[^"\r\n]*"|//[^\r\n]*',
                  lambda m: '' if m[0].startswith('//') else m[0], text)
    chunks = re.split(r'(?im)^\s*\$Class Name:\s*"([^"]+)"', text)
    result = set()
    for name, body in zip(chunks[1::2], chunks[2::2]):
        for flags in re.findall(r'(?im)^\s*\$Flags:\s*\(([^)]*)\)', body):
            if 'collectable' in [s.lower() for s in re.findall(r'"([^"]*)"', flags)]:
                result.add(name.lower())
    if not result:
        raise ValueError('No installed collectable classes parsed')
    return result


def nearest(rows, origin, limit):
    ranked = [dict(row, distance=math.dist(row['position'], origin))
              for row in rows]
    # Stable sort preserves authored order at equal distance.
    ranked.sort(key=lambda row: row['distance'])
    return [dict(row, distance=round(row['distance'], 4)) for row in ranked[:limit]]


def entity_behavior_fields(game):
    text = read_entry(game / 'tables.vpp', 'entity.tbl').decode('cp1252')
    text = re.sub(r'"[^"\r\n]*"|//[^\r\n]*',
                  lambda m: '' if m[0].startswith('//') else m[0], text)
    chunks = re.split(r'(?im)^\s*\$Name:\s*"([^"]+)"', text)
    fields = {}
    for name, body in zip(chunks[1::2], chunks[2::2]):
        lines = [line.strip() for line in body.splitlines() if line.strip()]
        selected = [line for line in lines if re.match(
            r'(?i)(\$(Movemode|Flags|Attack Style|Default [^:]+|AI[^:]*|Behavior[^:]*):|\+Action:.*rock_)', line)]
        fields[name.lower()] = selected
    return fields


def scan(game, limit):
    source = (ROOT / 'src/core/event.c').read_text()
    table = source.split('event_names[90]={', 1)[1].split('};', 1)[0]
    types = re.findall(r'"([^"]+)"', table)
    if len(types) != 90 or types[27] != 'Drop_Point_Marker':
        raise ValueError('Event vocabulary changed; inspect parser compatibility')
    eligible = collectable_classes(game)
    behavior = entity_behavior_fields(game)
    levels = []
    scanned = 0
    modes = {}
    for archive in sorted(game.glob('*.vpp')):
        with archive.open('rb') as stream:
            for entry in entries(stream):
                if not entry['name'].lower().endswith('.rfl'):
                    continue
                meta = inspect_level(stream, entry)
                scanned += 1
                if meta['version'] != 180:
                    raise ValueError((entry['name'], 'Unsupported version', meta['version']))

                def payload(kind):
                    section = next((s for s in meta['sections'] if s['type'] == kind), None)
                    if section is None:
                        return struct.pack('<I', 0)
                    stream.seek(entry['offset'] + section['offset'] + 8)
                    data = stream.read(section['size'])
                    if len(data) != section['size']:
                        raise ValueError('Truncated section')
                    return data

                events = inspect_events(payload('0x600'), types)
                collecting = [e for e in events if e['type_index'] == 34 and e['words'][0] == 3]
                for event in events:
                    if event['type_index'] == 34:
                        key = str(event['words'][0])
                        modes[key] = modes.get(key, 0) + 1
                actors = {}
                for uid, name, raw in entity_records(payload('0x30000')):
                    pos = struct.unpack_from('<3f', raw, 6 + struct.unpack_from('<H', raw, 4)[0])
                    actors[uid] = dict(uid=uid, class_name=name, position=list(pos))
                markers = [dict(uid=e['uid'], position=e['position'], name=e['name'])
                           for e in events if e['type_index'] == 27]
                clutter = [dict(uid=c['uid'], class_name=c['class_name'].decode('cp1252'),
                                position=list(struct.unpack('<3f', c['position'])),
                                authored_enabled=c['enabled'])
                           for c in inspect_clutter(payload('0x50000'))
                           if c['class_name'].decode('cp1252').lower() in eligible]
                grabbers = [a for a in actors.values() if 'grabber' in a['class_name'].lower()]
                if not collecting and not markers and not clutter and not grabbers:
                    continue
                grabbers = [dict(a, class_behavior_fields=behavior.get(a['class_name'].lower(), []),
                                 nearest_markers=nearest(markers, a['position'], limit),
                                 nearest_collectable_clutter=nearest(clutter, a['position'], limit))
                            for a in grabbers]
                cases = []
                for event in collecting:
                    linked = []
                    for uid in event['links']:
                        actor = actors.get(uid)
                        if actor:
                            linked.append(dict(actor, nearest_markers=nearest(markers, actor['position'], limit),
                                               nearest_collectable_clutter=nearest(clutter, actor['position'], limit)))
                    cases.append(dict(event_uid=event['uid'], name=event['name'],
                                      position=event['position'], delay=event['delay'],
                                      words=event['words'], flags=event['flags'], links=event['links'],
                                      linked_actors=linked,
                                      non_actor_links=[uid for uid in event['links'] if uid not in actors]))
                levels.append(dict(archive=archive.name, level=entry['name'], bytes=entry['size'],
                                   actor_count=len(actors), event_count=len(events),
                                   collectable_clutter_count=len(clutter), drop_markers=markers,
                                   collectable_clutter=clutter, grabbers=grabbers, collecting_events=cases))
    return dict(scanned_levels=scanned, matching_levels=len(levels),
                collecting_event_count=sum(len(l['collecting_events']) for l in levels),
                authored_ai_mode_counts=modes,
                collectable_classes=sorted(eligible),
                grabber_class_fields={name: fields for name, fields in behavior.items() if 'grabber' in name},
                levels=levels,
                scope='Actual installed records; Set_AI_Mode authored enum3 maps to collecting action5. '
                      'Class collectable flag is bit0. Nearest objects are spatial candidates only; '
                      'activation, residency, enabled state, navigation and hauling remain unverified. '
                      'No collecting events does not exclude class- or instance-initialized collecting.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=ROOT / 'Installed_Game')
    parser.add_argument('--nearest', type=int, default=3)
    args = parser.parse_args()
    if args.nearest < 1:
        parser.error('--nearest must be positive')
    print(json.dumps(scan(args.game, args.nearest), indent=2))


if __name__ == '__main__':
    main()

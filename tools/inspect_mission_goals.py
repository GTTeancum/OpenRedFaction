"""Verify mission-goal inventory against the current shared C level reader.

Run inspect_events.py first to refresh the independent original-layout inventory.
This verifies authored fields and cross-section references, not action semantics.
"""
import json
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/events.json').read_text())
goals = []
for level in inventory['results']:
    selected = [e for e in level['records'] if e['type_index'] in (35, 36, 37)]
    if not selected:
        continue
    raw = subprocess.check_output([
        str(root / 'build/pc/Release/rf_level_entity_probe.exe'),
        str(root / 'Installed_Game' / level['archive']), level['file'], '--events'])
    count, = struct.unpack_from('<I', raw)
    at = 4
    actual = {}
    for _ in range(count):
        fields = struct.unpack_from('<15I', raw, at)
        record = raw[at:at + 1144]
        actual[fields[0]] = record
        at += 1144 + fields[4] * 4
    assert at == len(raw)
    for e in selected:
        record = actual[e['uid']]
        assert list(struct.unpack_from('<4I', record, 24)) == e['flags'] + e['words']
        for offset, text in zip((60, 316, 572, 828), (e['type'], e['name'], *e['texts'])):
            assert record[offset:offset + 256].split(b'\0')[0].decode('cp1252') == text
        goals.append(dict(level=level['file'], **e))
declarations = {}
for e in goals:
    if e['type_index'] == 35:
        declarations.setdefault(e['name'].lower(), []).append(e['level'])
cross_section = [dict(level=e['level'], uid=e['uid'], name=e['texts'][0],
                      declared_in=declarations.get(e['texts'][0].lower(), []))
                 for e in goals if e['type_index'] != 35
                 and e['level'] not in declarations.get(e['texts'][0].lower(), [])]
report = dict(result='PASS', events=len(goals), declarations=sum(e['type_index'] == 35 for e in goals),
              levels=len({e['level'] for e in goals}), cross_section=cross_section, records=goals,
              scope='Current C reader matches independent authored goal names, flags and words; action and flag semantics are separate.')
(root / 'artifacts/mission-goals.json').write_text(json.dumps(report, indent=2) + '\n')
print({k: v for k, v in report.items() if k not in ('records', 'cross_section')})
print('Cross-section references:', len(cross_section))

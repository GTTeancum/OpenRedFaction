"""Inventory authored factory services before binding live clutter creation.

Uses the actual owned definition reader; counts declarations, not successfully
resolved effects or original runtime calls. No missing service is stubbed out.
"""
import collections
import json
import struct
import subprocess
from pathlib import Path
from inspect_clutter_records import sections, inspect

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())['files']
entry = next(e for a in inventory if a['path'] == 'tables.vpp'
             for e in a['vpp']['entries'] if e['name'].lower() == 'clutter.tbl')
with (root / 'Installed_Game/tables.vpp').open('rb') as f:
    f.seek(entry['offset'])
    raw = f.read(entry['size'])
path = root / 'artifacts/clutter-factory-requirements.tbl'
path.write_bytes(raw)
rows = next(inspect(data) for level, data in sections()
            if level['file'].lower() == 'l1s1.rfl')
groups = collections.OrderedDict()
for row in rows:
    name = row['class_name'].decode('cp1252')
    groups.setdefault(name.lower(), {'name': name, 'uids': []})['uids'].append(row['uid'])
counts = collections.Counter()
for group in groups.values():
    output = subprocess.check_output([
        str(root / 'build/pc/Release/rf_entity_assets_probe.exe'),
        '--clutter-definition', str(path), group['name']])
    status, = struct.unpack_from('<I', output)
    group['reader_status'] = status
    if status:
        counts['unresolved_placements'] += len(group['uids'])
        continue
    assert len(output) == 1576
    data = output[4:]
    names = [data[i:i+64].split(b'\0', 1)[0].decode('cp1252') for i in range(0, 1536, 64)]
    emitters, kind, flags = struct.unpack_from('<3I', data, 1536)
    lifetime, life, radius, width, height, fields = struct.unpack_from('<3f3I', data, 1548)
    group.update(model=names[1], corpse=names[2], sound=names[4],
                 explosion=names[5], glare=names[6], rod=names[7],
                 emitters=names[8:8+emitters], kind=kind, flags=flags,
                 life=life, emitter_lifetime=lifetime, radius=radius,
                 resource_fields=fields, screen=[width, height])
    requirements = dict(sound=bool(fields & 1), explosion=bool(fields & 2),
                        glare=bool(fields & 4), rod=bool(fields & 8),
                        emitter=bool(emitters), screen=bool(flags & 8),
                        light_tag=bool(flags & 16), collision=bool(flags & 6),
                        corpse=bool(names[2]))
    group['declared_services'] = requirements
    counts['resolved_placements'] += len(group['uids'])
    for service, required in requirements.items():
        counts[service] += len(group['uids']) * required
report = dict(level='L1S1.rfl', placements=len(rows), counts=dict(counts),
              classes=list(groups.values()),
              scope='Authored factory-facing metadata from the shared reader. '
                    'Service counts precede resource resolution and model-tag lookup; '
                    'they do not prove effect creation, eligibility or runtime call counts.')
(root / 'artifacts/clutter-factory-requirements.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report['counts'], indent=2))

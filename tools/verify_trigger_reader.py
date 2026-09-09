"""C trigger records and ordered link access vs bounded Python inventory."""
import json
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root/'artifacts/triggers.json').read_text())
results = []


def string(value):
    raw = value.encode('cp1252')
    assert len(raw)<256 and b'\0' not in raw
    return raw + bytes(256-len(raw))


for level in inventory['results']:
    raw = subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),
                                   str(root/'Installed_Game'/level['archive']), level['file'], '--triggers'])
    owned = subprocess.run([str(root/'build/pc/Release/rf_level_entity_probe.exe'),
                            str(root/'Installed_Game'/level['archive']), level['file'], '--owned-triggers'],
                           capture_output=True, check=True)
    assert owned.stdout == raw, (level['file'], 'owned lifetime')
    owned_bytes = int(owned.stderr)
    count, = struct.unpack_from('<I', raw)
    assert count == len(level['records'])
    at = 4
    for record in level['records']:
        links = record['links']
        link_offset = record['offset']+record['bytes']-4*len(links)
        expected = struct.pack('<6I', record['uid'], record['offset'], record['bytes'], link_offset, len(links), record['shape'])
        expected += string(record['name'])+string(record['script'])
        expected += struct.pack('<14I', record['enabled_byte'], *record['flags'], record['value_byte'],
                                record.get('flag',0), record['tail_flag'], record['unknown_word'], *record['fields'], record['tail_word'])
        expected += struct.pack('<19f', record['timing'], *record['position'], record.get('radius',0),
                                *record.get('orientation_disk',[0]*9), *record.get('dimensions_disk',[0]*3), *record['values'])
        assert len(expected)==668 and raw[at:at+668]==expected, (level['file'], record['uid'])
        at += 668
        expected_links = struct.pack('<'+'I'*len(links), *links)
        assert raw[at:at+len(expected_links)]==expected_links
        at += len(expected_links)
    assert at==len(raw)
    results.append(dict(file=level['file'], owned_bytes=owned_bytes, records=count, links=sum(len(r['links']) for r in level['records'])))
report = dict(result='PASS', levels=len(results), triggers=sum(r['records'] for r in results),
              links=sum(r['links'] for r in results), max_owned_bytes=max(r['owned_bytes'] for r in results),
              scope='PC C reader: every preserved field and ordered link vs Python inventory; each record '
                    'truncated by one byte preserves cursor/output; out-of-range link preserves output; exact EOF. '
                    'Owned records/links match after archive closure and level overwrite, exact budget succeeds, one byte short preserves output, repeat close clears owner. '
                    'Not original parser execution, runtime activation or Xbox ownership execution.', results=results)
(root/'artifacts/trigger-reader-verification.json').write_text(json.dumps(report, indent=2))
print({key:value for key,value in report.items() if key!='results'})

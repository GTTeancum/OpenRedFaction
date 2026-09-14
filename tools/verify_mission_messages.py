"""Compare every installed English dialogue entry with the shared C reader."""
import json, re, struct, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
commands = bytearray()
expected = []
for archive in inventory['files']:
    if 'vpp' not in archive:
        continue
    for entry in archive['vpp']['entries']:
        if not entry['name'].lower().endswith('_text.tbl'):
            continue
        with (root / 'Installed_Game' / archive['path']).open('rb') as f:
            f.seek(entry['offset']); raw = f.read(entry['size'])
        text = raw.decode('cp1252')
        headers = list(re.finditer(r'(?m)^\s*(\d+)\s+"([^"\r\n]*)"', text))
        for i, h in enumerate(headers):
            block = text[h.end():headers[i+1].start() if i+1 < len(headers) else len(text)]
            message = re.search(r'En:\s*"((?:\\.|[^"\\])*)"', block)[1]
            message = re.sub(r'\\([nrt"\\])', lambda m: {'n':'\n','r':'\r','t':'\t','"':'"','\\':'\\'}[m[1]], message)
            commands += struct.pack('<II', len(raw), int(h[1])) + raw
            expected.append((entry['name'], int(h[1]), h[2], message))
out = subprocess.check_output([str(root/'build/pc/Release/rf_mission_message_tests.exe'), '--parse'], input=commands)
assert len(out) == len(expected)*584, (len(out), len(expected))
for i, (table, uid, voice, subtitle) in enumerate(expected):
    row = out[i*584:(i+1)*584]
    status, actual_id = struct.unpack_from('<iI', row)
    actual_voice = row[8:72].split(b'\0', 1)[0].decode('cp1252')
    actual_text = row[72:].split(b'\0', 1)[0].decode('cp1252')
    assert (status, actual_id, actual_voice, actual_text) == (0, uid, voice, subtitle), (table, uid, status)
report = dict(result='PASS', entries=len(expected), tables=len({x[0] for x in expected}),
    scope='All installed English dialogue IDs, filenames and text through shared C reader; not runtime presentation or original parser parity.')
(root/'artifacts/mission-message-reader.json').write_text(json.dumps(report, indent=2))
print(report)

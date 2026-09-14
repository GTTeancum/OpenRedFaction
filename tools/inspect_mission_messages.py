"""Read-only inventory of per-level dialogue IDs, subtitles and voice assets."""
import hashlib, json, re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
events = json.loads((root / 'artifacts/events.json').read_text())['results']
assets = {e['name'].lower() for a in inventory['files'] if 'vpp' in a for e in a['vpp']['entries']}
rows = []
for archive in inventory['files']:
    if 'vpp' not in archive:
        continue
    for entry in archive['vpp']['entries']:
        if not entry['name'].lower().endswith('_text.tbl'):
            continue
        with (root / 'Installed_Game' / archive['path']).open('rb') as stream:
            stream.seek(entry['offset'])
            raw = stream.read(entry['size'])
        assert len(raw) == entry['size']
        text = raw.decode('cp1252')
        headers = list(re.finditer(r'(?m)^\s*(\d+)\s+"([^"\r\n]*)"', text))
        messages = {}
        for i, header in enumerate(headers):
            uid = int(header[1])
            assert uid not in messages, (entry['name'], uid)
            block = text[header.end():headers[i+1].start() if i+1 < len(headers) else len(text)]
            english = re.search(r'En:\s*"((?:\\.|[^"\\])*)"', block)
            assert english, (entry['name'], uid)
            messages[uid] = dict(voice=header[2], text=english[1])
        level = entry['name'][:-9] + '.rfl'
        authored = next((x['records'] for x in events if x['file'].lower() == level.lower()), [])
        refs = [(x['uid'], x['words'][0]) for x in authored if x['type'] == 'Message']
        rows.append(dict(level=level, table=entry['name'], archive=archive['path'], bytes=len(raw),
            sha256=hashlib.sha256(raw).hexdigest(), messages=len(messages), message_events=len(refs),
            max_text_bytes=max((len(x['text'].encode('cp1252')) for x in messages.values()), default=0),
            missing_ids=[dict(event=e, message=m) for e,m in refs if m not in messages],
            missing_voices=[dict(message=m, voice=x['voice']) for m,x in messages.items()
                if x['voice'] and x['voice'].lower() != 'none' and x['voice'].lower() not in assets]))
covered = {x['level'].lower() for x in rows}
missing_tables = [x['file'] for x in events if x['file'].lower() not in covered
                  and any(e['type'] == 'Message' for e in x['records'])]
report = dict(missing_tables=missing_tables, tables=len(rows), messages=sum(x['messages'] for x in rows),
    message_events=sum(x['message_events'] for x in rows), max_table_bytes=max(x['bytes'] for x in rows),
    max_text_bytes=max(x['max_text_bytes'] for x in rows), rows=rows,
    scope='Installed data inventory only; not runtime dialogue, queue or original parser validation.')
(root / 'artifacts/mission-messages.json').write_text(json.dumps(report, indent=2))
print(json.dumps({k:v for k,v in report.items() if k != 'rows'}))
print('Missing IDs:', sum(len(x['missing_ids']) for x in rows), 'Missing voices:', sum(len(x['missing_voices']) for x in rows))

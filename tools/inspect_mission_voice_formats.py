"""Inventory installed mission WAV formats and whole-file residency pressure."""
import collections,json,re,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
archives=json.loads((root/'artifacts/inventory.json').read_text())['files']
assets={e['name'].lower():(a['path'],e) for a in archives for e in a.get('vpp',{}).get('entries',[])}
names=set()
for a in archives:
 for e in a.get('vpp',{}).get('entries',[]):
  if not e['name'].lower().endswith('_text.tbl'):continue
  with (root/'Installed_Game'/a['path']).open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
  names.update(n.decode('cp1252').lower() for n in re.findall(rb'(?m)^\s*\d+\s+"([^"\r\n]*)"',raw))
silent_reference='' in names;names.discard('')
rows=[];missing=[];formats=collections.Counter()
for name in sorted(names):
 if name not in assets:missing.append(name);continue
 archive,e=assets[name]
 with (root/'Installed_Game'/archive).open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
 assert raw[:4]==b'RIFF' and raw[8:12]==b'WAVE',name
 p=12;fmt=None;data_bytes=0
 while p+8<=len(raw):
  tag=raw[p:p+4];size=struct.unpack_from('<I',raw,p+4)[0];assert p+8+size<=len(raw),name
  if tag==b'fmt ':fmt=struct.unpack_from('<HHIIHH',raw,p+8)
  if tag==b'data':data_bytes+=size
  p+=8+size+(size&1)
 assert fmt and data_bytes,name
 code,channels,rate,byte_rate,align,bits=fmt;formats[str((code,channels,rate,bits))]+=1
 rows.append(dict(name=name,archive=archive,file_bytes=e['size'],seconds=data_bytes/byte_rate,format=list(fmt)))
report=dict(subtitle_only_reference=silent_reference,unique_references=len(names),available=len(rows),missing=missing,formats=dict(formats),largest=sorted(rows,key=lambda x:x['file_bytes'],reverse=True)[:10],over_1MiB=[r for r in rows if r['file_bytes']>1048576],scope='Installed WAV header inventory; whole-file size excludes bank metadata and device copies; not decoder or playback verification.')
(root/'artifacts/mission-voice-formats.json').write_text(json.dumps(report,indent=2))
print({k:v for k,v in report.items() if k not in ('largest','over_1MiB')});print('over_1MiB',len(report['over_1MiB']),'largest',report['largest'][0])

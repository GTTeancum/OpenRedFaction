"""Authored metadata precedence inventory; no waveform loading."""
import contextlib,io,json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with contextlib.redirect_stdout(io.StringIO()):
 foley=runpy.run_path(str(root/'tools/verify_foley_table.py'))
 sound=runpy.run_path(str(root/'tools/inspect_sound_table.py'))
seen={};overlaps=[];duplicates=0
for row in sound['rows']:
 values=struct.unpack('<3f',struct.pack('<3f',row['near'],row['volume'],row['rolloff']))
 seen[row['name'].lower()]=dict(name=row['name'],parameters=list(values),source='sounds.tbl')
for i,row in enumerate(foley['samples']):
 name,near,volume,rolloff=struct.unpack('<61s3x3f',row);name=name.split(b'\0')[0].decode();key=name.lower()
 if not name:continue
 prior=seen.get(key)
 if prior:
  duplicates+=1
  if prior['source']=='sounds.tbl':overlaps.append(dict(foley_sample=i,name=name,retained=prior['parameters'],ignored=[near,volume,rolloff]))
 else:seen[key]=dict(name=name,parameters=[near,volume,rolloff],source='foley.tbl')
inventory=json.loads((root/'artifacts/inventory.json').read_text())['files']
archive=next(a for a in inventory if a['path']=='audio.vpp')
archive_names={e['name'].lower() for e in archive['vpp']['entries']}
registered={row['name'].lower():i for i,row in enumerate(sound['rows'])}
assert all(name in archive_names for name in registered)
ids=[];missing=[]
for raw in foley['samples']:
 name=struct.unpack('<61s3x3f',raw)[0].split(b'\0',1)[0].decode();key=name.lower()
 if key not in archive_names:missing.append(name);ids.append(-1);continue
 if key not in registered:registered[key]=len(registered)
 ids.append(registered[key])
hash_value=2166136261
for byte in struct.pack('<'+'i'*len(ids),*ids):hash_value=((hash_value^byte)*16777619)&0xffffffff
report=dict(result='PASS',global_declarations=len(sound['rows']),foley_declarations=len(foley['samples']),distinct_names=len(seen),duplicate_foley_declarations=duplicates,global_overlaps=overlaps,archive_present_names=len(registered),missing_foley=missing,sample_id_hash=hash_value,scope='Independent declaration inventory with binary32 parameters and case-insensitive first-name retention. Startup ordering and registration semantics are verified separately. Directory presence predicts sample IDs and their hash for the live metadata-bank check; does not prove waveform validity, level ordering or PCM residency.')
(root/'artifacts/foley-registration-inventory.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

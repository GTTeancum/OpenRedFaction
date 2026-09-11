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
report=dict(result='PASS',global_declarations=len(sound['rows']),foley_declarations=len(foley['samples']),distinct_names=len(seen),duplicate_foley_declarations=duplicates,global_overlaps=overlaps,scope='Independent declaration inventory with binary32 parameters and case-insensitive first-name retention. Startup ordering and registration semantics are verified separately. Does not prove archive presence, successful backend IDs, level ordering or PCM residency.')
(root/'artifacts/foley-registration-inventory.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

"""Independently derive opening appearance order and compare scene telemetry."""
import json,re,struct,sys
from pathlib import Path
from inspect_clutter_records import sections,inspect
from inspect_models import inspect as inspect_model
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())['files']
def read(archive,name):
 a=next(a for a in inventory if a['path']==archive)
 e=next(e for e in a['vpp']['entries'] if e['name'].lower()==name.lower())
 with (root/'Installed_Game'/archive).open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
raw=read('tables.vpp','clutter.tbl').decode('cp1252')
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw)
matches=list(re.finditer(r'(?im)^\s*\$Class\s+Name:\s*"([^"]+)"',text));classes={}
for i,m in enumerate(matches):
 block=text[m.end():matches[i+1].start() if i+1<len(matches) else len(text)]
 model=re.search(r'(?i)\$V3D\s+Filename:\s*"([^"]*)"',block)[1];skins={}
 for s in re.finditer(r'(?i)\$Skin:\s*"([^"]+)"\s*\(([^)]*)\)',block):
  skins.setdefault(s[1].lower(),re.findall(r'"([^"]*)"',s[2]))
 classes.setdefault(m[1].lower(),(model,skins))
rows=next(inspect(data) for level,data in sections() if level['file'].lower()=='l1s1.rfl')
keys=[];appearances=[];slots=[];named=0;missing=0;replacement_rows=0;h=2166136261
def hash_bytes(data):
 global h
 for b in data:h=((h^b)*16777619)&0xffffffff
for row in rows:
 cls=row['class_name'].decode('cp1252');skin=row['resource_name'].decode('cp1252')
 if cls.lower() not in classes:slots.append(0xffffffff);continue
 model,skins=classes[cls.lower()]
 if model.lower().endswith('.vfx'):slots.append(0xffffffff);continue
 compiled=model.rsplit('.',1)[0]+'.v3m';key=(compiled.lower(),cls.lower(),skin.lower())
 if key not in keys:
  keys.append(key)
  count=sum(s['materials'] for s in inspect_model(read('meshes.vpp',compiled))['sections'] if s['type']=='0x5355424d')
  textures=skins.get(skin.lower(),[])[:count] if skin else []
  missing+=bool(skin and skin.lower() not in skins);replacement_rows+=len(textures)
  hash_bytes(row['class_name']+b'\0');hash_bytes(row['resource_name']+b'\0')
  for name in textures:hash_bytes(name.encode('cp1252')+b'\0')
  appearances.append(dict(model=compiled,class_name=cls,skin=skin,materials=count,replacements=textures))
 slots.append(keys.index(key));named+=bool(skin)
hash_bytes(struct.pack('<'+'I'*len(slots),*slots))
expected=[len(keys),sum(s!=0xffffffff for s in slots),named,replacement_rows,missing,h]
log=Path(sys.argv[1]) if len(sys.argv)>1 else root/'artifacts/clutter-skins-pc.log'
line=next(l for l in log.read_text().splitlines() if l.startswith('CLUTTER_SKINS '))
actual=list(map(int,line.split()[1:]));assert actual==expected,(actual,expected)
report=dict(result='PASS',level='L1S1.rfl',expected=expected,appearances=appearances,
 scope='Independent archive record and table parsing, first case-insensitive class/skin selection, compiled model material counts, appearance order, sparse replacements and placement mapping hash versus PC scene telemetry. No glare runtime or original renderer equivalence claim.')
(root/'artifacts/clutter-scene-skins.json').write_text(json.dumps(report,indent=2));print(report)

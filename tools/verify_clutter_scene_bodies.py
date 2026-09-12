"""Derive initial prop-body counts/budgets directly from authored records."""
import json,re,sys
from pathlib import Path
from inspect_clutter_records import sections,inspect
from inspect_models import inspect as inspect_model
root=Path(__file__).resolve().parents[1];inventory=json.loads((root/'artifacts/inventory.json').read_text())['files']
def read(archive,name):
 a=next(a for a in inventory if a['path']==archive)
 e=next(e for e in a['vpp']['entries'] if e['name'].lower()==name.lower())
 with (root/'Installed_Game'/archive).open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
raw=read('tables.vpp','clutter.tbl').decode('cp1252')
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw)
matches=list(re.finditer(r'(?im)^\s*\$Class\s+Name:\s*"([^"]+)"',text));classes={}
for i,m in enumerate(matches):
 block=text[m.end():matches[i+1].start() if i+1<len(matches) else len(text)]
 model=re.search(r'(?i)\$V3D\s+Filename:\s*"([^"]*)"',block)[1]
 flags=re.findall(r'"([^"]*)"',re.search(r'(?i)\$Flags:\s*\(([^)]*)\)',block)[1])
 classes.setdefault(m[1].lower(),(model,set(f.lower() for f in flags)))
rows=next(inspect(data) for level,data in sections() if level['file'].lower()=='l1s1.rfl')
models={};bodies=[]
for row in rows:
 key=row['class_name'].decode('cp1252').lower()
 if key not in classes:continue
 model,flags=classes[key]
 if model.lower().endswith('.vfx'):continue
 compiled=(model.rsplit('.',1)[0]+'.v3m').lower()
 if compiled not in models:models[compiled]=sum(s['type']=='0x43535048' for s in inspect_model(read('meshes.vpp',compiled))['sections'])
 enabled=bool(flags&{'collide_weapon','collide_object'});count=models[compiled]
 bodies.append((enabled,max(1,count) if enabled else 0,count))
resident=len(rows)*4+len(models)*12+20;peak=resident
for enabled,spheres,source in bodies:
 used=532+spheres*24;peak=max(peak,resident+used+source*24);resident+=used
expected=[len(bodies),sum(v[0] for v in bodies),sum(v[1] for v in bodies),resident,peak]
log=Path(sys.argv[1]) if len(sys.argv)>1 else root/'artifacts/clutter-bodies-pc.log'
actual=list(map(int,next(l for l in log.read_text().splitlines() if l.startswith('CLUTTER_BODIES ')).split()[1:]))
assert actual[:5]==expected,(actual,expected)
assert actual[6:]==[len(bodies),len(bodies),0,0]
report=dict(result='PASS',expected=expected,models=len(models),scope='Independent L1S1 record/class flags and serialized CSPH counts; verified generic constructor storage sizes derive created/physics/sphere counts and measured budget. PC load/retirement counters agree. Does not independently verify every live body float or original global registration order.')
(root/'artifacts/clutter-scene-bodies.json').write_text(json.dumps(report,indent=2));print(report)

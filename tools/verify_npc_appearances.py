"""Check authored NPC appearance bindings against independently parsed tables."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game'
classes={a['name'].lower():a for a in json.loads((root/'artifacts/entity-assets-verification.json').read_text())['assets']};results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--skeletons',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'meshes.vpp'),level],text=True)
 keys={};count=0;owner=None
 for line in out.splitlines():
  f=line.split('\t')
  if f[0]=='APPEARANCE':
   c=classes[f[1].lower()];skins={n.lower():v for n,v in c['skins'].items()}
   expected=skins[f[2].lower()] if f[2] else []
   assert f[4].lower()==(c['model'].rsplit('.',1)[0]+'.v3c').lower() and f[5:]==expected,(level,f)
   key=(f[4].lower(),tuple(t.lower() for t in expected));idx=int(f[3])
   if idx in keys:assert keys[idx]==key
   else:keys[idx]=key
   count+=1
  elif line.startswith('APPEARANCES '):owner=list(map(int,line.split()[1:]))
 assert len(set(keys.values()))==len(keys)==owner[1]
 results.append(dict(level=level,skeletal_actors=count,appearances=owner[1],resident_bytes=owner[2],peak_bytes=owner[3]))
report=dict(result='PASS',scope='Every authored skeletal actor class/skin binding, ordered replacements and appearance deduplication against independent entity.tbl inventory; exact/one-byte-short budgets and repeated close. No runtime skin switching, images or GPU submission.',results=results)
(root/'artifacts/npc-appearances.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

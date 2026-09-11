"""Independent canonical state/action registration order for installed levels."""
import json,subprocess,re,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
actions=json.loads((root/'artifacts/entity-actions.json').read_text())['rows']
states=json.loads((root/'artifacts/entity-state-verification.json').read_text())['declarations']
names=json.loads((root/'artifacts/animation-names.json').read_text())
state_names=[x['name'] for x in names['states']['names']];action_names=[x['name'] for x in names['actions']['names']]
state_rows={(r['entity_class'].lower(),r['state'].lower()):r['motion'] for r in states if not r['weapon']}
action_rows={(r['entity_class'].lower(),r['action'].lower()):r for r in actions if not r['weapon']}
all_states={(r['entity_class'].lower(),r['weapon'].lower(),r['state'].lower()):r['motion'] for r in states}
all_actions={(r['entity_class'].lower(),r['weapon'].lower(),r['action'].lower()):r for r in actions}
reports=[]
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--catalog-fixture'],text=True).strip()=='CATALOG_FIXTURE PASS'
weapons=json.loads((root/'artifacts/weapon-names.json').read_text())['names']
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
parts=re.split(r'\$Name:\s*"([^"\r\n]+)"',text)
groups={name.lower():re.findall(r'\+Weapon\s+Specific:\s*"([^"\r\n]*)"',body) for name,body in zip(parts[1::2],parts[2::2])}
# Footstep pairs are read independently from the original class bodies.
footsteps={}
for cls,body in zip(parts[1::2],parts[2::2]):
 weapon=''
 for match in re.finditer(r'\+Weapon\s+Specific:\s*"([^"\r\n]*)"|\+State:\s*"([^"\r\n]*)"\s*"([^"\r\n]*)"',body):
  if match[1] is not None:weapon=match[1].lower();continue
  tail=body[match.end():]
  pair=re.match(r'\s*\+Footstep\s+Trigger:\s*([^\s]+)\s+([^\s]+)',tail)
  if pair:footsteps[cls.lower(),weapon,match[2].lower()]=[struct.unpack('<f',struct.pack('<f',float(v)))[0] for v in pair.groups()]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--catalog',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),str(root/'Installed_Game/motions.vpp'),str(root/'Installed_Game/meshes.vpp'),level],text=True)
 registries={};checked=0;group_registries={};group_slots=0;identity_count=0
 for line in out.splitlines():
  if line.startswith('IDENTITY\t'):
   _,cls,weapon,index,loop,authored=line.split('\t');cls=cls.lower();weapon=weapon.lower()
   cache={};resources=[]
   for looping,canonical,rows in ((1,state_names,all_states),(0,action_names,all_actions)):
    for name in canonical:
     row=rows.get((cls,weapon,name));motion=(row if looping else row['motion']) if row else ''
     if not motion:continue
     stem=motion.rsplit('.',1)[0].lower();first=cache.setdefault(stem,motion);key=(stem,looping)
     if key not in [r[0] for r in resources]:resources.append((key,first))
   expected=resources[int(index)]
   assert (authored,int(loop))==(expected[1],expected[0][1]),(level,cls,weapon,index,authored,expected)
   identity_count+=1;continue
  if line.startswith(('GROUP_STATE\t','GROUP_ACTION\t')):
   fields=line.split('\t');kind,cls,weapon,number,index,file=fields[:6];cls=cls.lower();weapon=weapon.lower();number=int(number);index=int(index)
   key=(cls,weapon);resources=group_registries.setdefault(key,[])
   if kind=='GROUP_STATE':motion=all_states.get((cls,weapon,state_names[number]),'');loop=1
   else:
    row=all_actions.get((cls,weapon,action_names[number]));motion=row['motion'] if row else '';loop=0
    assert fields[6]==(row['sound'] if row else ''),(level,key,number,'sound')
   if motion:
    identity=(motion.rsplit('.',1)[0].lower(),loop)
    if identity not in resources:resources.append(identity)
    assert index==resources.index(identity),(level,key,number,index,resources.index(identity))
    assert file.lower()==motion.split('.',1)[0].lower()+'.rfa'
   else:assert index==-1 and file==''
   group_slots+=1;continue
  if line.startswith('WEAPON_GROUPS\t'):
   _,cls,low,high=line.split('\t');mask=0
   for weapon in groups[cls.lower()]:mask|=1<<[w.lower() for w in weapons].index(weapon.lower())
   assert int(low)|(int(high)<<32)==mask,(level,cls,low,high,mask)
   continue
  if not line.startswith('ACTION\t'):continue
  _,cls,number,index,file,sound=line.split('\t');cls=cls.lower();number=int(number);index=int(index)
  if cls not in registries:
   resources=[]
   for state in state_names:
    motion=state_rows.get((cls,state),'')
    if motion:
     key=(motion.rsplit('.',1)[0].lower(),1)
     if key not in resources:resources.append(key)
   registries[cls]=resources
  resources=registries[cls];row=action_rows.get((cls,action_names[number]));motion=row['motion'] if row else ''
  expected_sound=row['sound'] if row else '';assert sound==expected_sound,(level,cls,number,sound,expected_sound)
  if motion:
   key=(motion.rsplit('.',1)[0].lower(),0)
   if key not in resources:resources.append(key)
   assert index==resources.index(key),(level,cls,number,index,resources.index(key))
   assert file.lower()==motion.split('.',1)[0].lower()+'.rfa'
  else:assert index==-1 and file==''
  checked+=1
 # Independently register authored groups in factory order into shared model maps.
 catalog_maps={};catalog_resources={};class_order=[];model_for={}
 for line in out.splitlines():
  if line.startswith('CATALOG_MAP\t'):
   fields=line.split('\t');cls,weapon=fields[1].lower(),fields[2].lower();model=int(fields[3])
   assert (cls,weapon) not in catalog_maps
   catalog_maps[cls,weapon]=list(map(int,fields[4:]))
   if not weapon:class_order.append(cls);model_for[cls]=model
  elif line.startswith('CATALOG_RESOURCE\t'):
   _,model,index,loop,identity,file=line.split('\t')
   assert (int(model),int(index)) not in catalog_resources
   catalog_resources[int(model),int(index)]=(int(loop),identity,file.lower())
 model_keys={};model_names={};expected_maps={};expected_resources={}
 for cls in class_order:
  model=model_for[cls]
  if model==0xffffffff:
   expected_maps[cls,'']=[-1]*68;continue
  keys=model_keys.setdefault(model,[]);cache=model_names.setdefault(model,{})
  for weapon in sorted(groups[cls],key=lambda w:[n.lower() for n in weapons].index(w.lower()))+['']:
   weapon=weapon.lower();mapping=[]
   for loop,canonical,rows in ((1,state_names,all_states),(0,action_names,all_actions)):
    for name in canonical:
     row=rows.get((cls,weapon,name));motion=(row if loop else row['motion']) if row else ''
     if not motion:mapping.append(-1);continue
     stem=motion.rsplit('.',1)[0].lower();identity=cache.setdefault(stem,motion);key=(stem,loop)
     if key not in keys:
      expected_resources[model,len(keys)]=(loop,identity,identity.split('.',1)[0].lower()+'.rfa');keys.append(key)
     mapping.append(keys.index(key))
   expected_maps[cls,weapon]=mapping
 assert catalog_maps==expected_maps,(level,'shared mappings')
 assert catalog_resources==expected_resources,(level,'shared resources')
 shared_markers={}
 for cls in class_order:
  if model_for[cls]==0xffffffff:continue
  for state in state_names:
   motion=all_states.get((cls,'',state),'');pair=footsteps.get((cls,'',state))
   if motion and pair:
    stem=motion.rsplit('.',1)[0].lower()
    shared_markers.setdefault(stem,tuple(int(frame*struct.unpack('<f',bytes.fromhex('8988083d'))[0]*30.*160.) for frame in pair))
 marked=0;marker_rows=0
 for line in out.splitlines():
  if not line.startswith('CATALOG_MARKERS\t'):continue
  _,model,index,mask,left,right=line.split('\t');resource=catalog_resources[int(model),int(index)]
  ticks=shared_markers.get(resource[1].rsplit('.',1)[0].lower())
  assert (int(mask),int(left),int(right))==((3,*ticks) if ticks else (0,0,0)),(level,model,index,resource,ticks,line)
  marker_rows+=1;marked+=ticks is not None
 assert marker_rows==len(catalog_resources)

 catalog_summary=next(line for line in out.splitlines() if line.startswith('CATALOG '))
 summary=next(line for line in out.splitlines() if line.startswith('BASE_MOTIONS '))
 assert group_slots==len(group_registries)*68
 assert identity_count==sum(map(len,registries.values()))+sum(map(len,group_registries.values()))
 assert int(next(line for line in out.splitlines() if line.startswith('BOUND_GROUPS ')).split()[1])==len(group_registries)
 reports.append(dict(level=level,action_slots=checked,retained_identities=identity_count,weapon_groups=len(group_registries),weapon_slots=group_slots,summary=summary,catalog_summary=catalog_summary,shared_resources=len(catalog_resources),marked_resources=marked))
report=dict(result='PASS',levels=reports,scope='Installed base and weapon-group canonical states then45 actions, local deduplication includes looping flag, filenames, retained first-authored cache names and sound labels exact. Shared per-model maps independently checked against authored declarations in weapon-before-base order. Shared base-state footstep masks/ticks checked independently across models and loop registrations. Original global cache order and live playback excluded.')
(root/'artifacts/base-action-sets.json').write_text(json.dumps(report,indent=2));print(report)

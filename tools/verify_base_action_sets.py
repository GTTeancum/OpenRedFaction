"""Independent canonical state/action registration order for installed levels."""
import json,subprocess,re
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
weapons=json.loads((root/'artifacts/weapon-names.json').read_text())['names']
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
parts=re.split(r'\$Name:\s*"([^"\r\n]+)"',text)
groups={name.lower():re.findall(r'\+Weapon\s+Specific:\s*"([^"\r\n]*)"',body) for name,body in zip(parts[1::2],parts[2::2])}
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--base-motions',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),str(root/'Installed_Game/motions.vpp'),level],text=True)
 registries={};checked=0;group_registries={};group_slots=0
 for line in out.splitlines():
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
 summary=next(line for line in out.splitlines() if line.startswith('BASE_MOTIONS '))
 assert group_slots==len(group_registries)*68
 assert int(next(line for line in out.splitlines() if line.startswith('BOUND_GROUPS ')).split()[1])==len(group_registries)
 reports.append(dict(level=level,action_slots=checked,weapon_groups=len(group_registries),weapon_slots=group_slots,summary=summary))
report=dict(result='PASS',levels=reports,scope='Installed base and weapon-group canonical states then45 actions, local deduplication includes looping flag, filenames and sound labels exact. Cross-group/global original registry ordering and live playback excluded.')
(root/'artifacts/base-action-sets.json').write_text(json.dumps(report,indent=2));print(report)

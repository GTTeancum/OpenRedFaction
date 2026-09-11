"""Independent canonical state/action registration order for installed levels."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
actions=json.loads((root/'artifacts/entity-actions.json').read_text())['rows']
states=json.loads((root/'artifacts/entity-state-verification.json').read_text())['declarations']
names=json.loads((root/'artifacts/animation-names.json').read_text())
state_names=[x['name'] for x in names['states']['names']];action_names=[x['name'] for x in names['actions']['names']]
state_rows={(r['entity_class'].lower(),r['state'].lower()):r['motion'] for r in states if not r['weapon']}
action_rows={(r['entity_class'].lower(),r['action'].lower()):r for r in actions if not r['weapon']}
reports=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--base-motions',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),str(root/'Installed_Game/motions.vpp'),level],text=True)
 registries={};checked=0
 for line in out.splitlines():
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
 reports.append(dict(level=level,action_slots=checked,summary=summary))
report=dict(result='PASS',levels=reports,scope='Installed base canonical states then45 actions, local deduplication includes looping flag, filenames and sound labels exact. Weapon-first/global original registry ordering and live playback excluded.')
(root/'artifacts/base-action-sets.json').write_text(json.dumps(report,indent=2));print(report)

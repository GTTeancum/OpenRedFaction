"""Check canonical state IDs, motion deduplication, reference counts and failures."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
declarations=json.loads((root/'artifacts/entity-state-verification.json').read_text())['declarations']
states=[n['name'] for n in json.loads((root/'artifacts/animation-names.json').read_text())['states']['names']]
classes=[a['name'] for a in json.loads((root/'artifacts/entity-assets-verification.json').read_text())['assets']]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
motion_entries={e['name'].lower():e['name'] for a in inventory['files'] if a['path']=='motions.vpp' for e in a['vpp']['entries']}
groups={(c,''):{} for c in classes}
for r in declarations:groups.setdefault((r['entity_class'],r['weapon']),{})[r['state'].lower()]=r['motion']
def run(cls,weapon):
    return subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--state-set',
        str(root/'Installed_Game/tables.vpp'),str(root/'Installed_Game/motions.vpp'),cls.upper(),weapon.upper()],text=True).splitlines()
passed=0;missing=[];registered=0;references=0
for (cls,weapon),rows in sorted(groups.items()):
    keys=[];names=[];counts=[];ids=[];failed=False
    for state in states:
        name=rows.get(state,'')
        if not name:ids.append(-1);continue
        key=name.rsplit('.',1)[0].lower() if '.' in name else name.lower()
        if key not in keys:
            compiled=name.split('.',1)[0].lower()+'.rfa'
            if compiled not in motion_entries:failed=True;break
            keys.append(key);names.append(motion_entries[compiled]);counts.append(0)
        index=keys.index(key);counts[index]+=1;ids.append(index)
    actual=run(cls,weapon)
    if failed:assert actual==['-3'],(cls,weapon,actual);missing.append([cls,weapon]);continue
    expected=['0',str(len(keys)),' '.join(map(str,ids))]+[f'{name} {count}' for name,count in zip(names,counts)]
    assert actual==expected,(cls,weapon,actual,expected)
    passed+=1;registered+=len(keys);references+=sum(counts)
for cls,weapon in [('missing-class',''),('miner1','missing-weapon')]:assert run(cls,weapon)==['-3']
report=dict(result='PASS',groups=len(groups),resolved_groups=passed,missing_groups=missing,
    registered_motions=registered,state_references=references,absent_group_guards=2,
    scope='Installed exact groups in canonical 23-state order; cache-stem deduplication, reference counts, file names and transactional missing-input/budget guards. No original complete registration-loop or playback equivalence claim.')
(root/'artifacts/entity-state-sets.json').write_text(json.dumps(report,indent=2));print(report)

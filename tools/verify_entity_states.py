"""Check named state declarations independently of the C token reader.

No claim about original parser, weapon fallback, state defaults or playback.
"""
import json,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
archive=next(a for a in inventory['files'] if a['path']=='tables.vpp')
entry=next(e for e in archive['vpp']['entries'] if e['name'].lower()=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:
    f.seek(entry['offset']);raw=f.read(entry['size']).decode('cp1252')
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw)
classes=list(re.finditer(r'(?im)^\s*\$Name:\s*"([^"]+)"',text))
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
cases=[];missing=[];motion_names={e['name'].lower() for a in inventory['files'] if a['path']=='motions.vpp' for e in a['vpp']['entries']}
for i,cls in enumerate(classes):
    block=text[cls.end():classes[i+1].start() if i+1<len(classes) else len(text)]
    weapons=list(re.finditer(r'(?im)^\s*\+Weapon\s+Specific:\s*"([^"]+)"',block))
    groups=[('',block[:weapons[0].start()] if weapons else block)]
    groups.extend((m[1],block[m.end():weapons[j+1].start() if j+1<len(weapons) else len(block)]) for j,m in enumerate(weapons))
    for weapon,body in groups:
        seen=set()
        for match in re.finditer(r'(?im)^\s*\+State:\s*"([^"]+)"\s*"([^"]*)"',body):
            state,motion=match.groups();assert state.lower() not in seen,(cls[1],weapon,state);seen.add(state.lower())
            actual=subprocess.check_output([probe,'--state',str(root/'Installed_Game/tables.vpp'),cls[1].upper(),weapon.upper(),state.upper()],text=True).splitlines()
            assert actual==['0',motion],(cls[1],weapon,state,actual,motion)
            item=dict(entity_class=cls[1],weapon=weapon,state=state,motion=motion)
            cases.append(item)
            # Inventory observation only: compiled-extension conversion already
            # documented separately; no runtime alias or fallback is invented.
            if motion and motion.rsplit('.',1)[0].lower()+'.rfa' not in motion_names:missing.append(item)
        actual=subprocess.check_output([probe,'--state',str(root/'Installed_Game/tables.vpp'),cls[1],weapon,'absent-state'],text=True).strip()
        assert actual=='-3',(cls[1],weapon,actual)
report=dict(result='PASS',classes=len(classes),state_selections=len(cases),missing_compiled_motions=missing,
    declarations=cases,scope='Installed base and exact weapon-specific declarations, ASCII-insensitive keys; no default/fallback or original parser equivalence claim')
(root/'artifacts/entity-state-verification.json').write_text(json.dumps(report,indent=2))
print({k:v for k,v in report.items() if k not in ('declarations','missing_compiled_motions')})
print('Missing compiled motion declarations:',len(missing))

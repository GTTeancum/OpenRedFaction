"""Compare authored-state bindings with independently indexed motion headers."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
rows=json.loads((root/'artifacts/entity-state-verification.json').read_text())['declarations']
inventory=json.loads((root/'artifacts/inventory.json').read_text())
archive=next(a for a in inventory['files'] if a['path']=='motions.vpp')
entries={e['name'].lower():e for e in archive['vpp']['entries']}
resolved=0;missing=[];empty=0
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
with (root/'Installed_Game/motions.vpp').open('rb') as original:
    for row in rows:
        authored=row['motion'];compiled=authored.split('.',1)[0]+'.rfa'
        actual=subprocess.check_output([probe,'--state-open',str(root/'Installed_Game/tables.vpp'),
            str(root/'Installed_Game/motions.vpp'),row['entity_class'],row['weapon'],row['state']],text=True).splitlines()
        entry=entries.get(compiled.lower()) if authored else None
        if entry is None:
            assert actual==['-3'],(row,actual)
            if authored:missing.append(row)
            else:empty+=1
        else:
            original.seek(entry['offset']);header=struct.unpack('<20I',original.read(80))
            assert actual==['0',entry['name'],f'{header[1]} {header[6]}'],(row,actual)
            resolved+=1
report=dict(result='PASS',declarations=len(rows),resolved=resolved,empty=empty,missing=missing,
    scope='Exact base/weapon selection, original first-dot compiled naming, archive lookup and validated header; no state registration, playback or fallback claim')
(root/'artifacts/state-motion-bindings.json').write_text(json.dumps(report,indent=2));print(report)

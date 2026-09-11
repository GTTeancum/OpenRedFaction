"""Compact runtime force owner against original-verified record construction."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/force-regions.json').read_text());results=[]
for level in inventory['results']:
    inputs=bytearray()
    for r in level['records']:
        inputs.extend(struct.pack('<6I',r['uid'],r['offset'],r['bytes'],r['header_byte'],r['shape'],r['flags'])+bytes(512))
        inputs.extend(struct.pack('<16f',*r['position'],*r['orientation_disk'],*(r['extent']+[0]*(3-len(r['extent']))),r['strength']))
    records=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-build'],input=inputs)
    owned=subprocess.run([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(root/'Installed_Game'/level['archive']),level['file'],'--runtime-forces'],capture_output=True,check=True)
    assert owned.stdout==struct.pack('<I',len(level['records']))+records,level['file']
    results.append(dict(file=level['file'],count=len(level['records']),bytes=int(owned.stderr)))
report=dict(result='PASS',levels=len(results),regions=sum(r['count'] for r in results),max_bytes=max(r['bytes'] for r in results),results=results,
 scope='PC compact runtime ownership matches original-verified constructor for every authored record; exact budget, one-byte-short budget, nonempty destination, truncated section rollback, archive-independent lifetime and repeat close. This is ownership testing, not original allocator equivalence or native XEMU evidence.')
(root/'artifacts/force-owner.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})

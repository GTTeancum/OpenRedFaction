"""Compare every owned C force record with the independent disk inventory."""
import json
import struct
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/force-regions.json').read_text())
results=[]
def string(text):
    value=text.encode('cp1252')
    assert len(value)<256 and b'\0' not in value
    return value+bytes(256-len(value))
for level in inventory['results']:
    run=subprocess.run([str(root/'build/pc/Release/rf_level_entity_probe.exe'),
        str(root/'Installed_Game'/level['archive']),level['file'],'--forces'],capture_output=True,check=True)
    expected=struct.pack('<I',len(level['records']))
    for r in level['records']:
        extent=r['extent']+[0]*(3-len(r['extent']))
        expected+=struct.pack('<6I',r['uid'],r['offset'],r['bytes'],r['header_byte'],r['shape'],r['flags'])
        expected+=string(r['name'])+string(r['label'])
        expected+=struct.pack('<16f',*r['position'],*r['orientation_disk'],*extent,r['strength'])
    assert run.stdout==expected,level['file']
    results.append(dict(file=level['file'],records=len(level['records']),owned_bytes=int(run.stderr)))
report=dict(result='PASS',levels=len(results),regions=sum(r['records'] for r in results),
    max_owned_bytes=max(r['owned_bytes'] for r in results),results=results,
    scope='PC C authored reader and owned storage vs Python inventory; each record truncated by one byte '
          'preserves cursor/output; exact EOF; exact budget succeeds and one byte short preserves output; '
          'nonempty destination rejected; data verified after archive close and level overwrite; repeat close. '
          'No original parser execution, runtime construction or native Xbox ownership execution.')
(root/'artifacts/force-reader-verification.json').write_text(json.dumps(report,indent=2))
print({k:v for k,v in report.items() if k!='results'})

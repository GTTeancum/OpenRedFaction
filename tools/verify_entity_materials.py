"""Compare bounded material reader with installed authored coefficients."""
import json,re,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for f in inventory['files'] if f['path']=='tables.vpp' for e in f['vpp']['entries'] if e['name']=='materials.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:
    f.seek(entry['offset']);text=f.read(entry['size']).decode('cp1252')
names=['default','rock','metal','flesh','water','lava','solid','sand','ice','glass']
tags=['elasticity','friction','density','bouyancy','traction']
blocks=re.split(r'(?i)\$name:\s*"([^"]+)"',text.split('#End')[0])
expected={}
for i in range(1,len(blocks),2):
    name=blocks[i].lower();values=[float(re.search(r'\$'+tag+r':\s*([^\s]+)',blocks[i+1],re.I)[1]) for tag in tags]
    expected[name]=struct.pack('<I5f',names.index(name),*values).hex()
assert set(expected)==set(names)
probe=root/'build/pc/Release/rf_entity_assets_probe.exe'
for name in names+['unknown','']:
    actual=subprocess.check_output([str(probe),'--material',str(root/'Installed_Game/tables.vpp'),name.upper()],text=True).strip()
    assert actual==expected.get(name,expected['default']),name
folder=root/'artifacts/material-reader-tests';folder.mkdir(exist_ok=True)
def check(text):
    payload=text.encode();size=4096+((len(payload)+2047)//2048)*2048;data=bytearray(size)
    struct.pack_into('<4I',data,0,0x51890ace,1,1,size)
    data[2048:2062]=b'materials.tbl\0';struct.pack_into('<I',data,2108,len(payload));data[4096:4096+len(payload)]=payload
    path=folder/'fixture.vpp';path.write_bytes(data)
    return subprocess.run([str(probe),'--material',str(path),'flesh'],capture_output=True)
valid='#Materials $name: "flesh" $elasticity: .2 $friction: .8 $density: 800 $bouyancy: .1 $traction: 1 #End'
assert check(valid).stdout.decode().strip()==expected['flesh']
bad=[valid.replace('800','nan'),valid.replace('800','1e999'),valid.replace('$traction: 1',''),valid.replace('$traction: 1','$traction: 1 $traction: 2'),valid.replace('.8','".8"'),valid.replace('"flesh"','"flesh')]
for text in bad:assert check(text).returncode==3
report=dict(result='PASS',installed_materials=10,lookup_cases=12,malformed_cases=len(bad),scope='Installed coefficients and bounded-reader output preservation; not original parser execution or live physics integration.')
(root/'artifacts/entity-materials-verification.json').write_text(json.dumps(report,indent=2));print(report)

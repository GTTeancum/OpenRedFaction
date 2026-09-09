"""Installed entity.tbl named sphere declarations versus shared metadata reader."""
import json,re,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];inv=json.loads((root/'artifacts/inventory.json').read_text())
e=next(e for a in inv['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);text=f.read(e['size']).decode('cp1252')
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],text)
classes=list(re.finditer(r'(?im)^\s*\$Name:\s*"([^"]+)"',text));number=r'([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)'
pattern=re.compile(r'\$Collision\s+Sphere:\s*"([^"]*)"\s*'+number+r'\s+'+number+r'(?:\s*\+radius:\s*'+number+r')?(?:\s*\+spring\s+constant:\s*'+number+r'\s*\+spring\s+length:\s*'+number+r')?',re.I)
probe=root/'build/pc/Release/rf_entity_assets_probe.exe';records=[];total=0
for i,c in enumerate(classes):
 block=text[c.end():classes[i+1].start() if i+1<len(classes) else len(text)];expected=[]
 for m in pattern.finditer(block):
  name,sp,mp,radius,spring,length=m.groups();expected.append(name.encode().ljust(24,bytes(1))+struct.pack('<5f',float(radius) if radius else -1,float(sp),float(mp),float(spring) if spring else -1,float(length) if length else 0))
 assert len(expected)==len(re.findall(r'\$Collision\s+Sphere:',block,re.I))
 actual=subprocess.check_output([str(probe),'--sphere-declarations',str(root/'Installed_Game/tables.vpp'),c[1].upper()],text=True).splitlines()
 assert int(actual[0])==len(expected) and actual[1:]==[v.hex() for v in expected],c[1]
 total+=len(expected);records.append(dict(name=c[1],count=len(expected),records=[v.hex() for v in expected]))
folder=root/'artifacts/sphere-declaration-tests';folder.mkdir(exist_ok=True)
def check(text):
 payload=text.encode();size=4096+((len(payload)+2047)//2048)*2048;data=bytearray(size);struct.pack_into('<4I',data,0,0x51890ace,1,1,size);data[2048:2059]=b'entity.tbl\0';struct.pack_into('<I',data,2108,len(payload));data[4096:4096+len(payload)]=payload
 path=folder/'fixture.vpp';path.write_bytes(data)
 return subprocess.run([str(probe),'--sphere-declarations',str(path),'actor'],capture_output=True).returncode
valid='$Name: "actor" $Collision Sphere: "a" .5 2 +radius: .75 +spring constant: 4 +spring length: .25'
assert check(valid)==0
bad=['$Name: "actor" $Collision Sphere: "a" .5',valid.replace('.5','nan'),valid.replace('"a"','"'+('a'*24)+'"'),valid.replace('+spring length: .25',''),valid.replace('+spring length: .25','+spring length: invalid'),'$Name: "actor" '+('$Collision Sphere: "a" .5 2 '*9)]
for t in bad:assert check(t)==3
report=dict(result='PASS',classes=len(records),declarations=total,malformed_cases=len(bad),records=records,scope='Installed table declarations, optional radius/spring values and case-insensitive class selection. Metadata reader, not complete original table parser or live entity integration.')
(root/'artifacts/sphere-declarations-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})

"""Check streamed CSPH records against installed V3C bytes and malformed inputs."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
probe=root/'build/pc/Release/rf_model_file_probe.exe'
inv=json.loads((root/'artifacts/inventory.json').read_text());records=[];total=0
for archive in inv['files']:
 for entry in archive.get('vpp',{}).get('entries',[]):
  if not entry['name'].lower().endswith('.v3c'):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as f:f.seek(entry['offset']);data=f.read(entry['size'])
  expected=[];spheres=[]
  for section in inspect(data)['sections']:
   if int(section['type'],16)!=0x43535048:continue
   at=section['offset']+8;raw=data[at:at+44];assert section['bytes']==52
   expected.append(raw[:24].hex()+''.join(f'{v:08x}' for v in struct.unpack_from('<5I',raw,24)))
   spheres.append(dict(name=raw[:24].split(bytes(1),1)[0].decode('ascii'),parent=struct.unpack_from('<i',raw,24)[0],center=struct.unpack_from('<3f',raw,28),radius=struct.unpack_from('<f',raw,40)[0]))
  actual=subprocess.check_output([str(probe),str(path),entry['name'],'--spheres'],text=True).splitlines()
  assert actual==expected,entry['name'];total+=len(spheres);records.append(dict(model=entry['name'],spheres=spheres))
folder=root/'artifacts/model-sphere-tests';folder.mkdir(exist_ok=True)
def check(raw):
 payload=struct.pack('<3I',0x5246434d,0x40000,0)+bytes(28)+struct.pack('<2I',0x43535048,len(raw))+raw+bytes(8)
 size=4096+((len(payload)+2047)//2048)*2048;archive=bytearray(size)
 struct.pack_into('<4I',archive,0,0x51890ace,1,1,size);archive[2048:2058]=b'model.v3c\0';struct.pack_into('<I',archive,2108,len(payload));archive[4096:4096+len(payload)]=payload
 path=folder/'fixture.vpp';path.write_bytes(archive)
 return subprocess.run([str(probe),str(path),'model.v3c','--spheres'],capture_output=True).returncode
raw=b'csphere_0'+bytes(15)+struct.pack('<i4f',-1,1,2,3,.5)
assert check(raw)==0
bad=[raw[:n] for n in range(44)]+[raw+bytes(1)]
for offset in (28,32,36,40):
 for value in (float('inf'),float('nan')):
  changed=bytearray(raw);changed[offset:offset+4]=struct.pack('<f',value);bad.append(bytes(changed))
changed=bytearray(raw);changed[24:28]=struct.pack('<i',-2);bad.append(bytes(changed))
changed=bytearray(raw);changed[40:44]=struct.pack('<f',-1);bad.append(bytes(changed))
for value in bad:assert check(value)!=0
report=dict(result='PASS',models=len(records),spheres=total,malformed_cases=len(bad),records=records,scope='Installed V3C CSPH fields versus shared PC streaming reader, with malformed payload checks. No original loader execution, bone transforms, class overrides or runtime actor integration.')
(root/'artifacts/model-spheres-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})

"""VFX texture binding against independently loaded archive images."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];probe=str(root/'build/pc/Release/rf_material_probe.exe');entity=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
w=lambda *x:struct.pack('<'+'I'*len(x),*x)
inv=json.loads((root/'artifacts/inventory.json').read_text());arc=next(a for a in inv['files'] if a['path']=='meshes.vpp');views=[]
for a in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 e=next(e for e in arc['vpp']['entries'] if e['name']==a['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as file:file.seek(e['offset']);data=file.read(e['size'])
 at=a['header_bytes'];v=int(a['version'],16)
 while at<len(data):
  tag,n=struct.unpack_from('<II',data,at);d=data[at+8:at+4+n];rows=[]
  if tag==0x4c54414d:rows.append(subprocess.check_output([entity,'--vfx-material'],input=w(v,len(d))+d))
  if tag==0x4f584653 and v<0x40000:
   pre=subprocess.check_output([entity,'--vfx-mesh-prefix'],input=w(v,0,len(d))+d);flags,samples=struct.unpack_from('<II',pre,148);pos=struct.unpack_from('<I',pre,168)[0];count=struct.unpack_from('<I',d,pos)[0];pos+=4
   for _ in range(count):
    row=subprocess.check_output([entity,'--vfx-embedded-material'],input=w(v,flags,samples,len(d)-pos)+d[pos:]);rows.append(row);pos+=struct.unpack_from('<I',row,204)[0]
  for row in rows:assert row[:4]==w(0);views.append(row[4:212])
  at+=4+n
archives=['maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp']
def run(rows,budget,paths=archives):
 return subprocess.check_output([probe,'--vfx-textures',str(budget),*[str(root/'Installed_Game'/a) for a in paths]],input=w(len(rows))+b''.join(rows)).decode().splitlines()
rows=run(views,4000000);header=list(map(int,rows[0].split()));assert header[:2]==[0,len(views)]
resident=header[3];assert run(views,resident)==rows and run(views,resident-1)==['-4 0 0 0']
names=[];bindings=[]
for view in views:
 mask=struct.unpack_from('<I',view,204)[0];binding=[]
 for j,off in enumerate((20,72,144)):
  name=view[off:off+33].split(b'\0')[0].decode()
  if mask&(1<<j) and name:
   if name.lower() not in [n.lower() for n in names]:names.append(name)
   binding.append([n.lower() for n in names].index(name.lower()))
  else:binding.append(0xffffffff)
 bindings.append(binding)
assert header[2]==len(names)==16
for line,binding in zip(rows[1:1+len(views)],bindings):assert line=='B '+' '.join(map(str,binding))
at=1+len(views);frames=0;expected=24+len(views)*264
for name in names:
 single=list(map(int,subprocess.check_output([probe,'--particle-animation',name,'4000000',*[str(root/'Installed_Game'/a) for a in archives]]).split()))
 assert single[0]==0;_,owner,image_owner,n,rate,used,index=single[:7];expected+=used-20
 assert rows[at]==f'T {name} {n} {rate} {index} {used}';at+=1
 for j in range(n):assert rows[at]=='F '+' '.join(map(str,single[7+j*3:10+j*3]));at+=1;frames+=1
assert at==len(rows) and expected==resident
missing=run(views,4000000,['tables.vpp']);assert missing[0]==f'0 {len(views)} 16 {24+len(views)*264}'
assert all(line=='B 4294967295 4294967295 4294967295' for line in missing[1:1+len(views)])
duplicate=bytearray(views[0]);duplicate[20:53]=bytes(views[0][20:53]).upper();assert run(views+[bytes(duplicate)],4000000)[0]==f'0 {len(views)+1} 16 {resident+264}'
invalid=bytearray(views[0]);invalid[204:208]=w(8);assert run([bytes(invalid)],4000000)==['-2 0 0 0']
invalid=bytearray(views[0]);invalid[20:53]=b'x'*33;invalid[204:208]=w(1);assert run([bytes(invalid)],4000000)==['-2 0 0 0']
report=dict(result='PASS',materials=len(views),textures=len(names),frames=frames,resident_bytes=resident,scope='PC archive-backed bindings and all pixel hashes against independent image loads after source/archive retirement; exact/short budgets, missing requests, case dedup and malformed views. Xbox build only; no native XEMU/GPU binding or frame clock.')
(root/'artifacts/vfx-textures.json').write_text(json.dumps(report,indent=2));(root/'artifacts/vfx-texture-views.bin').write_bytes(w(len(views))+b''.join(views));print(report)

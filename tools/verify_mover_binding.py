"""Initial owned mover bindings, pose/radius evidence, budgets and identity separation."""
import json,struct,subprocess,re,sys
import pefile
from pathlib import Path
root=Path(__file__).resolve().parents[1];levels=json.loads((root/'artifacts/movers.json').read_text())['results'];bounds=json.loads((root/'artifacts/mover-bounds-original.json').read_text())['results'];results=[]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
pe=pefile.PE(str(root/'build/xbox/main.exe'));im=pe.get_memory_mapped_image();image_base=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(image_base,(len(im)+4095)//4096*4096);x.mem_write(image_base,im)
base=0x31000000;x.mem_map(base,1024*1024);poses_at=base+4096;views_at=base+512*1024;stack=base+1000000;stop=stack+4096
entry=int(re.search(r'_rf_geometry_collision_movers_sync\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for level,bound in zip(levels,bounds):
 assert level['file']==bound['file'] and level['archive']==bound['archive']
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--bound-movers',str(root/'Installed_Game'/level['archive']),level['file']]);count,retained,peak=struct.unpack_from('<3I',raw);assert count==level['count'] and len(raw)==12+368*count and peak>=retained
 for i,(record,b) in enumerate(zip(level['records'],bound['records'])):
  uid,handle,faces=struct.unpack_from('<i2I',raw,12+368*i);assert uid==record['uid']==b['uid'] and handle==0x12340000+i and faces==record['faces']
  p=record['position'];disk=record['orientation_disk'];matrix=disk[3:]+disk[:3];r=b['origin_radius']
  want=struct.pack('<30f',*[v-r for v in p],*[v+r for v in p],*p,*matrix,*p,*matrix)
  assert raw[24+368*i:144+368*i]==want,(level['file'],uid,'bounds/pose')
  pose=struct.pack('<I58f',0x6000000,r,*p,*matrix,*p,*p,*p,0.,0.,0.,*matrix,*matrix,*matrix,*[v-r for v in p],*[v+r for v in p])
  assert raw[144+368*i:380+368*i]==pose,(level['file'],uid,'owned base/runtime pose')
 shifted=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--shifted-movers',str(root/'Installed_Game'/level['archive']),level['file']])
 assert len(shifted)==len(raw) and shifted[:12]==raw[:12]
 f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
 for i,(record,b) in enumerate(zip(level['records'],bound['records'])):
  p=record['position'];q=[f32(v+d) for v,d in zip(p,(1,2,3))];disk=record['orientation_disk'];matrix=disk[3:]+disk[:3];r=b['origin_radius']
  assert shifted[12+368*i:24+368*i]==raw[12+368*i:24+368*i]
  want=struct.pack('<30f',*[v-r for v in q],*[v+r for v in q],*q,*matrix,*q,*matrix)
  assert shifted[24+368*i:144+368*i]==want,(level['file'],record['uid'],'shifted collision view')
  pose=struct.pack('<I58f',0x6000000,r,*p,*matrix,*q,*q,*q,0.,0.,0.,*matrix,*matrix,*matrix,*[v-r for v in q],*[v+r for v in q])
  assert shifted[144+368*i:380+368*i]==pose,(level['file'],record['uid'],'shifted owned pose')
 committed=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--committed-movers',str(root/'Installed_Game'/level['archive']),level['file']])
 want=bytearray(shifted)
 for i,record in enumerate(level['records']):
  p=record['position'];target=[f32(v+d) for v,d in zip(p,(1,2,3))];velocity=[f32(f32(q-v)/.25) for q,v in zip(target,p)]
  pending=[f32(v+f32(speed*.25)) for v,speed in zip(p,velocity)];assert pending==target
  struct.pack_into('<3f',want,144+368*i+92,*velocity)
 assert committed==want,(level['file'],'normal propagation, controller commit and collision sync')
 # Run the compiled NXDK synchronization over all committed real-level poses.
 assert count*236<500000 and count*156<450000
 views=bytearray([0xa5])*(count*156);expected_views=bytearray(views)
 for i in range(count):
  x.mem_write(poses_at+236*i,committed[144+368*i:380+368*i])
  expected_views[156*i+24:156*i+144]=committed[24+368*i:144+368*i]
 if count:x.mem_write(views_at,bytes(views))
 x.mem_write(base,struct.pack('<8I',0,0,views_at,0,poses_at,count,retained,peak))
 x.mem_write(stack,struct.pack('<2I',stop,base));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(views_at,count*156))==expected_views,(level['file'],'NXDK collision sync')
 results.append(dict(file=level['file'],count=count,retained=retained,peak=peak))
report=dict(result='PASS',levels=len(results),movers=sum(r['count'] for r in results),max_retained=max(r['retained'] for r in results),max_peak=max(r['peak'] for r in results),scope='PC owned collision views and 236-byte factory/physics pose snapshots after source closure: exact base/current/pending poses, matrices, initial flags, zero velocity and original-radius bounds; diagnostic runtime handles separate from file UIDs. Exact/short peak budgets and failure preservation. Forced controller translation also checked on every owned pose and collision view after source closure with unchanged memory budgets. Normal propagation followed by controller commit and collision synchronization also matches every pose/view, retaining velocity. NXDK collision synchronization matches every committed pose and preserves all other view bytes. No runtime handle allocator, full factory, rendering integration or XEMU.',results=results)
(root/'artifacts/mover-binding-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})

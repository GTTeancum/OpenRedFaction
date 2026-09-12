"""Authored prop glare bindings and original attachment poses versus scene owners."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,handle,data,stack,stop=[0x30000000+i*0x100000 for i in range(6)]
for a in (obj,desc,handle,data,stack,stop): u.mem_map(a,65536)
from inspect_models import inspect as inspect_model
from inspect_clutter_records import sections,inspect
from unicorn.x86_const import UC_X86_REG_EAX
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];archive=next(a for a in inv if a['path']=='meshes.vpp');entries={e['name'].lower():e for e in archive['vpp']['entries']}
classes={a['class_name'].lower():a['model'].lower() for a in json.loads((root/'artifacts/clutter-scene-skins.json').read_text())['appearances']}
rows=next(inspect(d) for l,d in sections() if l['file'].lower()=='l1s1.rfl')
models={};h=2166136261;queries=hits=0
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
def hashed(raw):
 global h
 for b in raw:h=((h^b)*16777619)&0xffffffff
u.mem_write(handle,w(1,obj));u.mem_write(obj+0x48,w(1,desc));u.mem_write(desc+0x8c,w(desc+0x200));u.mem_write(desc+0x204,w(desc+0x300));u.mem_write(0x20852f4,w(0))
definitions=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--glare-classes',str(root/'Installed_Game/tables.vpp'),'1000000'])[16:]
glare_names=[definitions[i:i+64].split(b'\0',1)[0].decode('cp1252') for i in range(0,len(definitions),300)]
factory=json.loads((root/'artifacts/clutter-factory-requirements.json').read_text())
base_classes={c['name'].lower():c.get('glare','') for c in factory['classes'] if c['reader_status']==0}
overrides={(c['class_name'].lower(),c['skin'].lower()):c['glare'] for c in json.loads((root/'artifacts/clutter-skin-glare.json').read_text())['authored_overrides']}
caches={};parents=requests=created=changed=0;records=[]
for row in rows:
 cls=row['class_name'].decode('cp1252').lower();model=classes.get(cls)
 if model is None:continue
 parents+=1
 if model not in models:
  e=entries[model]
  with (root/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
  lod=next(s for s in inspect_model(raw)['sections'] if s['type']=='0x5355424d')['lods'][0]
  models[model]=[raw[lod['attachment_offset']+100*i:lod['attachment_offset']+100*(i+1)] for i in range(lod['props'])]
 tags=models[model];u.mem_write(desc+0x310,w(desc+0x1000,len(tags)))
 if tags:u.mem_write(desc+0x1000,b''.join(tags))
 name=base_classes[cls];base=glare_names.index(name) if name in glare_names else -1
 if base>=0 and cls not in caches:
  cache=[];number=1
  while True:
   name=('corona_'+str(number)).encode();u.mem_write(data+512,name+b'\0');u.mem_write(stack+64000,w(stop,handle,data+512));u.reg_write(UC_X86_REG_ESP,stack+64000)
   u.emu_start(0x503220,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;index=u.reg_read(UC_X86_REG_EAX);queries+=1
   if index==0xffffffff:break
   if len(cache)<4:cache.append(index)
   number+=1;assert number<100
  caches[cls]=cache
 skin=overrides.get((cls,row['resource_name'].decode('cp1252').lower()));selected=glare_names.index(skin) if skin in glare_names else base
 for index in caches.get(cls,[]):
  requests+=1
  if base<0:continue
  created+=1;changed+=skin in glare_names
  height,length=struct.unpack_from('<2f',definitions,base*300+288);radius=max(height,length);radius=1.0 if radius<=0 else radius
  u.mem_write(data,row['matrix']+row['position']);u.mem_write(stack+64000,w(stop,handle,index,data,data+36,data+128,data+164));u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x27f)
  u.emu_start(0x5034f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;pose=bytes(u.mem_read(data+128,48))
  hashed(w(row['uid'],index,selected,0x6030000,0,1)+struct.pack('<f',radius)+pose)
  records.append(dict(uid=row['uid'],tag=index,base_class=base,final_class=selected,radius=radius,pose=pose.hex()))
entry=next(e for a in inv if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='clutter.tbl')
appearances=len(json.loads((root/'artifacts/clutter-scene-skins.json').read_text())['appearances'])
base_bytes=28+len(rows)*4*4;retained=base_bytes+created*528;peak=max(base_bytes+entry['size']+4164+appearances*4,retained+appearances*4)
expected=[parents,queries,requests,created,changed,retained,peak,h,created,0]
log=(root/'artifacts/glare-instances-pc.log').read_text();actual=list(map(int,next(l for l in log.splitlines() if l.startswith('GLARE_INSTANCES ')).split()[1:]));assert actual==expected,(actual,expected)
report=dict(result='PASS',expected=expected,records=records,scope='Authored placements and verified class/skin metadata, actual original503220 tag lookup and5034f0 world pose, original-verified four-entry class cache and radius/skin rules versus scene instance hash/count/budget/retirement. Registry identity checked in scene. Diagnostic creation order remains distinct from full original object scheduling; no glare rendering/frame clock claim.')
(root/'artifacts/clutter-scene-glares.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})

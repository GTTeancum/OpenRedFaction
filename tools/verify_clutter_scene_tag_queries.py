"""Compare static tag placement with unhooked original 0x5034f0."""
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
for row in rows:
 model=classes.get(row['class_name'].decode('cp1252').lower())
 if model is None:continue
 if model not in models:
  e=entries[model]
  with (root/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
  lod=next(s for s in inspect_model(raw)['sections'] if s['type']=='0x5355424d')['lods'][0]
  models[model]=[raw[lod['attachment_offset']+100*i:lod['attachment_offset']+100*(i+1)] for i in range(lod['props'])]
 tags=models[model];u.mem_write(desc+0x310,w(desc+0x1000,len(tags)))
 if tags:u.mem_write(desc+0x1000,b''.join(tags))
 names=[b'corona_1',b'corona_2',b'corona_3',b'corona_4',b'corona_5',b'corona_rod1',b'corona_rod2',b'light_prop']+[t[:68].split(b'\0',1)[0] for t in tags]
 for name in names:
  u.mem_write(data+512,name+b'\0');u.mem_write(stack+64000,w(stop,handle,data+512));u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x27f)
  u.emu_start(0x503220,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;index=u.reg_read(UC_X86_REG_EAX)
  queries+=1;hashed(w(row['uid'])+name+b'\0'+w(index))
  if index!=0xffffffff:
   u.mem_write(data,row['matrix']+row['position']);u.mem_write(stack+64000,w(stop,handle,index,data,data+36,data+128,data+164));u.reg_write(UC_X86_REG_ESP,stack+64000)
   u.emu_start(0x5034f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;hits+=1;hashed(bytes(u.mem_read(data+128,48)))
expected=[queries,hits,hits,h,0]
log=(root/'artifacts/clutter-tag-query-pc.log').read_text();actual=list(map(int,next(l for l in log.splitlines() if l.startswith('CLUTTER_TAG_QUERIES ')).split()[1:]));assert actual==expected,(actual,expected)
report=dict(result='PASS',expected=expected,scope='Actual original static lookup503220 and world placement5034f0 on serialized opening attachments and authored prop poses versus registered scene lookups/pose hash. Eight factory tag names plus each authored tag name for every retained prop; supplied original model storage, no original factory/effect creation claim.')
(root/'artifacts/clutter-scene-tag-queries.json').write_text(json.dumps(report,indent=2));print(report)

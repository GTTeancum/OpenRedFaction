"""Full original54de40 and all geometry helpers versus shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;batch=b+0x1000;vertices=b+0x2000;planes=b+0x3000;records=b+0x4000;query=b+0x5000;hit=b+0x6000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
import math
from inspect_models import inspect
sources=[];models=set();patterns=set()
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for resource in archive.get('vpp',{}).get('entries',[]):
  if not resource['name'].lower().endswith('.v3m'):continue
  with (root/'Installed_Game'/archive['path']).open('rb') as stream:stream.seek(resource['offset']);raw=stream.read(resource['size'])
  for section in inspect(raw)['sections']:
   for lod in section.get('lods',[]):
    start=lod['data_offset'];relative=(lod['batches']*56+15)&~15
    for batch_index in range(lod['batches']):
     v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,start+lod['data_bytes']+4+batch_index*18)
     sizes=[p,p,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0];offsets=[]
     for size in sizes:offsets.append(start+relative);relative=(relative+size+15)&~15
     for tri in range(t):
      plane=raw[offsets[4]+tri*16:offsets[4]+tri*16+16]
      if all(math.isfinite(n) for n in struct.unpack('<4f',plane)):continue
      ids=struct.unpack_from('<3h',raw,offsets[3]+tri*8);assert all(0<=n<v for n in ids)
      verts=b''.join(raw[offsets[0]+n*12:offsets[0]+n*12+12] for n in ids)
      assert all(math.isfinite(n) for n in struct.unpack('<9f',verts))
      sources.append((plane,verts));models.add(resource['name']);patterns.add(plane.hex())
assert sources
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
rng=random.Random(0x54de40);counts={}
for sphere in (False,True):
 name='sphere' if sphere else 'ray';entry=int(re.search(r'\s_rf_collision_model_'+name+r'_triangle\s+([0-9a-fA-F]+)',mapping)[1],16);commands=[];answers=[]
 for source,(plane,verts) in enumerate(sources):
  v=struct.unpack('<9f',verts);center=[sum(v[i::3])/3 for i in range(3)]
  for case in range(32):
   start=[n+rng.uniform(-2,2) for n in center];delta=[rng.uniform(-4,4) for _ in range(3)]
   if case%8==0:delta=[0,0,0]
   two=(0,1,0x20,0x100)[case%4];radius=(0,.0001,.25,1)[case//4%4]
   initial=f([(0,.25,.5,1)[case//8],11,12,13,14,15,16])+w(0x12345678)
   command=f(start+delta)+plane+verts+w(records,two)+initial+(f([radius]) if sphere else b'');commands.append(command)
   u.mem_write(batch,b'\x00'*0x38);u.mem_write(batch+4,w(vertices));u.mem_write(batch+0x10,w(planes,records));u.mem_write(planes,plane);u.mem_write(vertices,verts);u.mem_write(records,struct.pack('<4H',0,1,2,0x20))
   u.mem_write(query,b'\xa5'*104);u.mem_write(query+0x48,f([radius]));u.mem_write(query+0x50,f(start+delta));u.mem_write(hit,initial)
   before=bytes(u.mem_read(batch,0x5000));u.mem_write(stack,w(stop,0,batch,0,query,hit,two));u.reg_write(UC_X86_REG_ESP,stack)
   u.emu_start(0x54de40 if sphere else 0x54dd10,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
   expected=w(u.reg_read(UC_X86_REG_EAX)&255)+bytes(u.mem_read(hit,32));assert expected==w(0)+initial,(name,source,case)
   assert bytes(u.mem_read(batch,0x5000))==before;answers.append(expected)
   x.mem_write(b,command+b'\xa5'*16);args=[stop,b+24,b,b+12]
   if sphere:args.append(struct.unpack('<I',f([radius]))[0])
   args.extend([two,b+84]);x.mem_write(stack,w(*args));x.reg_write(UC_X86_REG_ESP,stack)
   x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
   actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+84,32));assert actual==expected,(name,source,case,actual.hex())
   assert bytes(x.mem_read(b,84))==command[:84] and bytes(x.mem_read(b+len(command),16))==b'\xa5'*16
 actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-'+name+'-triangle'],input=b''.join(commands));assert actual==b''.join(answers)
 counts[name]=len(commands)
report=dict(result='PASS',models=sorted(models),nonfinite_triangles=len(sources),plane_patterns=sorted(patterns),cases=counts,original_sha256=sha,scope='Every installed nonfinite static plane with authored finite vertices,32 finite queries each through full original thin/swept triangle and PC/NXDK. All reject and preserve hit/input. Constructor flags initialized only. No arbitrary nonfinite inputs, FPU status parity, part traversal or native XEMU claim.')
(root/'artifacts/model-nonfinite-planes.json').write_text(json.dumps(report,indent=2));print(report)

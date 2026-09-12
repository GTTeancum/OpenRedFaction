"""Authored skeletal LOD queries against original54e140, with prepared transforms."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;model=b+0x1000;count=model+0x48;query=b+0x2000;hit=b+0x3000;backend=b+0x4000;callback=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,0x200000);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
from inspect_models import inspect
u=machine(exe);u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
model=b;part=b+0x1000;table=b+0x4000;lp=b+0x8000;pose=b+0x10000;rawbase=b+0x20000
query=b+0x1f0000;hit=b+0x1f1000;stack=b+0x1fe000;stop=b+0x1ff000
u.mem_write(model+0x90,w(part));u.mem_write(part+0x8c,w(table));u.mem_write(table,w(1,lp))
def prepared(m,address,size,_):
 sp=m.reg_read(UC_X86_REG_ESP);assert m.reg_read(UC_X86_REG_ECX)==pose
 assert struct.unpack('<6I',m.mem_read(sp+4,24))==(0,model,0,0,0,1)
 m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+28)
u.hook_add(UC_HOOK_CODE,prepared,None,0x51ba00,0x51ba00)
def hash_bytes(data):
 h=2166136261
 for byte in data:h=((h^byte)*16777619)&0xffffffff
 return h
models=cases=hits=lods=0;plan=[]
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for resource in archive.get('vpp',{}).get('entries',[]):
  if not resource['name'].lower().endswith('.v3c'):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as stream:stream.seek(resource['offset']);raw=stream.read(resource['size'])
  assert len(raw)<0x1c0000;u.mem_write(rawbase,raw);commands=[];answers=[];lod_index=0
  for section in inspect(raw)['sections']:
   for lod in section.get('lods',[]):
    assert lod['flags']&2
    u.mem_write(lp+8,w(rawbase+lod['data_offset']));u.mem_write(lp+12,struct.pack('<H',lod['batches']))
    start=lod['data_offset'];relative=(lod['batches']*56+15)&~15;positions=[];max_vertices=0
    for i in range(lod['batches']):
     v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,start+lod['data_bytes']+4+i*18)
     sizes=[p,p,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0];regions=[]
     for size in sizes:regions.append(rawbase+start+relative if size else 0);relative=(relative+size+15)&~15
     positions.extend(struct.iter_unpack('<3f',raw[regions[0]-rawbase:regions[0]-rawbase+v*12]));max_vertices=max(max_vertices,v)
     batch=rawbase+start+i*56
     for field,region in [(4,0),(8,1),(12,2),(16,4),(20,3),(24,5),(28,6),(36,7)]:u.mem_write(batch+field,w(regions[region]))
     u.mem_write(batch+40,struct.pack('<7H',v,t,p,ix,uv,links,extra))
    assert positions
    lo=[min(p[j] for p in positions) for j in range(3)];hi=[max(p[j] for p in positions) for j in range(3)]
    for mode in range(2):
     matrices=[value for bone in range(256) for value in (1,0,0,0,1,0,0,0,1,0,0,bone*.03125 if mode else 0)]
     u.mem_write(pose+0x960,f(matrices))
     for axis in range(3):
      for direction in (-1,1):
       for radius in (0,.25):
        start=[(lo[j]+hi[j])*.5 for j in range(3)];delta=[0,0,0]
        start[axis]=lo[axis]-2 if direction==1 else hi[axis]+2;delta[axis]=direction*(hi[axis]-lo[axis]+4)
        flags=2+mode;q=f([0,0,0,1,0,0,0,1,0,0,0,1]+start+delta+[radius])+w(flags)+b'\xa5'*24
        initial=f([1,11,12,13,14,15,16])+w(0x12345678);commands.append(q+initial+w(0,lod_index,mode))
        u.mem_write(query,q);u.mem_write(hit,initial);u.mem_write(0x1d0e9b8,b'\xa5'*(max_vertices*12));u.mem_write(stack,w(stop,pose,query,hit,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,model)
        u.emu_start(0x54e140,stop,count=3000000);assert u.reg_read(UC_X86_REG_EIP)==stop
        accepted=u.reg_read(UC_X86_REG_EAX)&255;answers.append(w(accepted)+bytes(u.mem_read(query,104))+bytes(u.mem_read(hit,32))+w(hash_bytes(u.mem_read(0x1d0e9b8,max_vertices*12))));hits+=accepted
    lod_index+=1;lods+=1
  actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),resource['name'],'--skin-trace'],input=b''.join(commands))
  assert len(actual)==len(answers)*144
  for n,expected in enumerate(answers):assert actual[n*144:n*144+144]==expected,(resource['name'],n,expected.hex(),actual[n*144:n*144+144].hex())
  plan.append(archive['path'].encode('ascii').ljust(64,b'\0')+resource['name'].encode('ascii').ljust(128,b'\0')+w(len(commands))+b''.join(a+e for a,e in zip(commands,answers)))
  models+=1;cases+=len(answers)
report=dict(result='PASS',models=models,lods=lods,cases=cases,hits=hits,original_sha256=sha,scope='All shipped skeletal LODs through original54e140/54e200 geometry versus PC archive-owned loader. Only51ba00 supplied with prepared identity/bone-dependent translation matrices; no authored animation evaluation.24 thin/sphere axis sweeps per LOD, closest/first hit, exact query/hit/return and full scratch hash. Original layout rebuilt; original loader, native XEMU and scene residency remain outside this gate.')
(root/'artifacts/model-skin-authored-trace.bin').write_bytes(w(0x52465354,models)+b''.join(plan))
(root/'artifacts/model-skin-authored-trace.json').write_text(json.dumps(report,indent=2));print(report)

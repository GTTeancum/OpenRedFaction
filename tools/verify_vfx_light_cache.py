"""VFX scale/quaternion/translation stages against original math helpers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;FACE=B+0x2000;UV=B+0x3000;PTR=B+0x4000;CTX=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x4d96c0);inputs=[];responses=[];hits=0
for i in range(2048):
 count=i%33;lo=[-rng.uniform(0,10) for _ in range(3)];hi=[rng.uniform(0,10) for _ in range(3)];sources=[]
 lo=list(struct.unpack('<3f',f(*lo)));hi=list(struct.unpack('<3f',f(*hi)))
 for j in range(count):
  kind=(i+j)%6;pos=[rng.uniform(-20,20) for _ in range(3)];end=pos if j%3==0 else [rng.uniform(-20,20) for _ in range(3)];color=[0,0,0] if j%5==0 else [1,.5,.25];enabled=j%4;cls=j%2;lr=rng.choice([0,1,3,8])
  if j%7==0:
   # Expanded corner and the adjacent inside/outside float on X.
   pos=list(struct.unpack('<3f',f(*[v+lr for v in hi])))
   pos[0]=struct.unpack('<f',w(struct.unpack('<I',f(pos[0]))[0]+[0,-1,1][i%3]))[0];end=pos
  sources.append(w(kind,0)+f(*pos,*end,0,0,0,*color,lr,0,0,1)+w(0)+bytes([enabled,cls,0,0]))
 data=w(count,100,1,101,32)+f(*lo,*hi)+b''.join(sources);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*(4*count));x.mem_write(CTX,w(100,0,32,OUT,1))
 assert call('rf_vfx_light_cache_refresh',[CTX,101,B+44,count,B+20,B+32])==0
 n=read(x,CTX+4);got=w(0,read(x,CTX),n,read(x,CTX+16))+bytes(x.mem_read(OUT,4*count));responses.append(got)
 for address,co,bo in [(0x4d96c0,0x1bc,8),(0x4d9870,0x360,0x48)]:
  o.mem_write(OWNER+co,w(0,32,PTR,100));o.mem_write(OWNER+bo,f(*lo,*hi));o.mem_write(0xc96874,w(101));o.mem_write(0x879af8,bytes([i%2]));head=0xc96768 if i%2 else 0xc4e6b8
  o.mem_write(head,w(FACE if count else head));o.mem_write(PTR,b'\xa5'*(4*count))
  for j,s in enumerate(sources):
   p=FACE+j*0x90;o.mem_write(p,bytes(0x90));o.mem_write(p,w(p+0x90 if j+1<count else head));o.mem_write(p+8,s[:4]);o.mem_write(p+12,s[8:44]);o.mem_write(p+0x3c,s[56:60]);o.mem_write(p+0x40,s[44:56]);o.mem_write(p+0x4c,s[76:78])
  o.mem_write(STACK,w(STOP,OWNER));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(address,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
  on=read(o,OWNER+co);expected=w(0,read(o,OWNER+co+12),on,1)+w(*[(read(o,PTR+j*4)-FACE)//0x90 for j in range(on)])+b'\xa5'*(4*(count-on))
  assert got==expected,(i,hex(address),got.hex(),expected.hex())
  # Unchanged generation bypasses the source list even after all types change.
  for j in range(count):o.mem_write(FACE+j*0x90+8,w(0))
  saved=bytes(o.mem_read(PTR,4*count));o.mem_write(STACK,w(STOP,OWNER));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(address,STOP,count=1000000);assert read(o,OWNER+co)==on and bytes(o.mem_read(PTR,4*count))==saved
  o.mem_write(0xc96874,w(102));o.mem_write(STACK,w(STOP,OWNER));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(address,STOP,count=1000000);assert read(o,OWNER+co)==0 and read(o,OWNER+co+12)==102
 saved=bytes(x.mem_read(OUT,4*count))
 for j in range(count):x.mem_write(B+44+j*80,w(0))
 assert call('rf_vfx_light_cache_refresh',[CTX,101,0,0,0,0])==0 and read(x,CTX+4)==n and bytes(x.mem_read(OUT,4*count))==saved
 assert call('rf_vfx_light_cache_refresh',[CTX,102,B+44,count,B+20,B+32])==0 and read(x,CTX+4)==0 and read(x,CTX)==102
 hits+=1
# Bad bounds and insufficient storage preserve the whole cache/output.
for data in [w(0,100,1,101,32)+f(1,0,0,0,0,0),w(1,100,1,101,0)+f(*([0]*6))+w(1,0)+f(*([0]*16))+w(0)+bytes(4)]:
 count=struct.unpack_from('<I',data)[0];inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*(4*count));initial=w(100,0,read(x,B+16),OUT,1);x.mem_write(CTX,initial);status=call('rf_vfx_light_cache_refresh',[CTX,101,B+44,count,B+20,B+32]);assert status!=0 and bytes(x.mem_read(CTX,20))==initial and bytes(x.mem_read(OUT,4*count))==b'\xa5'*(4*count);responses.append(w(status,100,0,1)+b'\xa5'*(4*count))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-light-cache'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_rebuild_cases_per_path=2048,paths=2,original_nxdk_cache_hit_and_invalidation_sequences=hits,guards=2,scope='Actual4d96c0/4d9870 linked-list traversal, geometry, collection clearing/append and generation fast paths. Preallocated capacity avoids original heap growth; no hooks. Caller supplies ordered sources and invalidation; null-room/global cache and native scene integration remain.')
(root/'artifacts/vfx-light-cache.json').write_text(json.dumps(report,indent=2));print(report)

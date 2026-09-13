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



rng=random.Random(0x4d9fd1);inputs=[];responses=[]
for i in range(1024):
 count=i%17;mode=i%2;transformed=(i//2)%2;flags=[(i//4)%2,(i//8)%2];sources=[]
 bounds=f(-8,-8,-8,8,8,8);center=[-3,-3,-3] if mode else [rng.uniform(-4,4) for _ in range(3)]
 query=w(mode,*flags,transformed)+f(*center,3,3,3,5,*[rng.uniform(-1,1) for _ in range(3)],*[((1 if j in (0,4,8) else 0)+rng.uniform(-.1,.1)) for j in range(9)])
 shading=f(*[rng.uniform(-3,3) for _ in range(3)],*[rng.uniform(-1,1) for _ in range(3)],.02,.04,.01,.25)
 for j in range(count):
  kind=1+(i+j)%4;pos=[rng.uniform(-15,15) for _ in range(3)];end=[rng.uniform(-15,15) for _ in range(3)];axis=[rng.uniform(-1,1) for _ in range(3)];color=[0,0,0] if j%5==0 else [rng.random()*.5 for _ in range(3)]
  sources.append(w(kind,j%4)+f(*pos,*end,*axis,*color,rng.choice([1,5,15]),rng.random(),-.5,.75)+w(j%2)+bytes([j%3,j%2,0,0]))
 data=w(count)+bounds+query+shading+b''.join(sources);inputs.append(data);x.mem_write(B,data);x.mem_write(CTX,w(0,0,32,PTR,0))
 assert call('rf_vfx_light_cache_refresh',[CTX,1,B+160,count,B+4,B+16])==0
 assert call('rf_vfx_lights_prepare',[B+160,count,CTX,B+28,UV,32,OUT+4])==0
 n=read(x,OUT+4);assert call('rf_vfx_lighting',[B+120,B+132,B+144,read(x,B+156),UV,n,OUT])==0
 got=bytes(x.mem_read(OUT,3));responses.append(w(0,n)+got)
 o.mem_write(B,data);o.mem_write(0x5a38c4,w(0));o.mem_write(0x5a38cc,w(1));o.mem_write(0x5a38c8,b'\1');o.mem_write(0xc96874,w(1));o.mem_write(0x879af8,b'\1');o.mem_write(0xc96768,w(FACE if count else 0xc96768));o.mem_write(OWNER+8,bounds);o.mem_write(OWNER+0x1bc,w(0,32,PTR,0))
 for j,s in enumerate(sources):
  p=FACE+j*0x90;o.mem_write(p,bytes(0x90));o.mem_write(p,w(p+0x90 if j+1<count else 0xc96768));o.mem_write(p+8,s[:4]);o.mem_write(p+12,s[8:44]);o.mem_write(p+0x38,s[60:64]);o.mem_write(p+0x3c,s[56:60]);o.mem_write(p+0x40,s[44:56]);o.mem_write(p+0x4c,s[76:78]);o.mem_write(p+0x4e,s[72:73]);o.mem_write(p+0x54,s[4:8]);o.mem_write(p+0x84,s[64:72])
 args=[OWNER,B+44,B+56,*flags] if mode else [OWNER,B+44,read(o,B+68),*flags]
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x4d9c00 if mode else 0x4d99c0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP and read(o,0xc9687c)==n
 o.mem_write(0x1818b84,w(transformed));o.mem_write(0x1818a28,query[44:56]);o.mem_write(0x1818a38,query[56:92]);o.mem_write(STACK,w(STOP));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x4d9fd0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 for j in range(n):
  p=read(o,0xc4d588+j*4);source=(p-FACE)//0x90;expected=bytearray(sources[source][:76]);kind=struct.unpack_from('<I',expected)[0]
  if transformed:
   expected[8:20]=o.mem_read(p+0x5c,12)
   if kind==4:expected[20:32]=o.mem_read(p+0x68,12)
   if kind==3:expected[32:44]=o.mem_read(p+0x74,12)
  assert bytes(x.mem_read(UV+j*76,76))==expected,(i,j,'prepared source')
 o.mem_write(0x5a38d4,shading[24:36]);o.mem_write(0x5a38e0,shading[36:40]);o.mem_write(STACK,w(STOP,OUT,OUT+1,OUT+2,1,B+120,B+132,0x40000000));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x4daff0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert got==bytes(o.mem_read(OUT,3)),(i,got.hex(),bytes(o.mem_read(OUT,3)).hex())
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-cached-lighting'],input=b''.join(inputs));assert pc==b''.join(responses)
# Corrupt cached index, insufficient output capacity, invalid query: no usable count.
for alter in ('index','capacity','query'):
 x.mem_write(CTX,w(1,1,32,PTR,1));x.mem_write(PTR,w(0));x.mem_write(B+28,w(0,1,1,0)+f(*([0]*19)));x.mem_write(OUT+4,w(123))
 if alter=='index':x.mem_write(PTR,w(count))
 if alter=='query':x.mem_write(B+28,w(2))
 assert call('rf_vfx_lights_prepare',[B+160,count,CTX,B+28,UV,0 if alter=='capacity' else 32,OUT+4])!=0 and read(x,OUT+4)==0
report=dict(result='PASS',original_pc_nxdk_pipeline_cases=1024,nxdk_guards=3,scope='Original cache rebuild4d96c0 -> sphere/box active selection ->4d9fd0 transforms ->4daff0 mixed shading, no hooks. Supplied ordered linked sources/room bounds/ambient; original preallocated cache. Shared PC/NXDK selected count and final RGB; NXDK also checks every prepared source field. Source-owner invalidation/global disabled state/edge caches/native drawing remain.')
(root/'artifacts/vfx-cached-lighting.json').write_text(json.dumps(report,indent=2));print(report)

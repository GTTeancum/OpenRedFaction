"""Complete original mixed scene-light accumulation and RGB conversion."""
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



rng=random.Random(0x4da933);inputs=[];responses=[]
o.mem_write(0x5a38c4,w(0));o.mem_write(0x5a38cc,w(1));o.mem_write(0x1818b84,w(1))
for i in range(2048):
 count=i%17;position=[rng.uniform(-5,5) for _ in range(3)];normal=[rng.uniform(-1,1) for _ in range(3)];ambient=[rng.random()*.1 for _ in range(3)];directional=.25;sources=[]
 for j in range(count):
  kind=1+(i+j)%4;profile=j%4;pos=[rng.uniform(-10,10) for _ in range(3)];end=[rng.uniform(-10,10) for _ in range(3)];axis=[rng.uniform(-1,1) for _ in range(3)];color=[rng.random()*.5 for _ in range(3)]
  sources.append(w(kind,profile)+f(*pos,*end,*axis,*color,rng.choice([0,1,5,20]),rng.random(),-.5,.75)+w(j%2))
 data=w(count)+f(*position,*normal,*ambient,directional)+b''.join(sources);transform=f(*[rng.uniform(-1,1) for _ in range(3)],*[((1 if j in (0,4,8) else 0)+rng.uniform(-.2,.2)) for j in range(9)]);inputs.append(data[:44]+transform+data[44:]);x.mem_write(B,data);x.mem_write(CTX,transform);
 for j in range(count):assert call('rf_vfx_light_transform',[B+44+j*76,CTX,CTX+12,B+44+j*76])==0
 x.mem_write(OUT,b'\xa5'*3);assert call('rf_vfx_lighting',[B+4,B+16,B+28,struct.unpack_from('<I',data,40)[0],B+44,count,OUT])==0;got=bytes(x.mem_read(OUT,3));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(0x5a38d4,data[28:40]);o.mem_write(0x5a38e0,data[40:44]);o.mem_write(0xc9687c,w(count))
 for j,s in enumerate(sources):
  p=FACE+j*0x90;o.mem_write(p,bytes(0x90));o.mem_write(p+8,s[:4]);o.mem_write(p+12,s[8:44]);o.mem_write(p+0x38,s[60:64]);o.mem_write(p+0x3c,s[56:60]);o.mem_write(p+0x40,s[44:56]);o.mem_write(p+0x4e,s[72:73]);o.mem_write(p+0x54,s[4:8]);o.mem_write(p+0x84,s[64:72]);o.mem_write(0xc4d588+j*4,w(p))
 o.mem_write(0x1818a28,transform[:12]);o.mem_write(0x1818a38,transform[12:]);o.mem_write(STACK,w(STOP));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x4d9fd0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 for j,s in enumerate(sources):
  p=FACE+j*0x90;expected=bytearray(s);kind=struct.unpack_from('<I',s)[0];expected[8:20]=o.mem_read(p+0x5c,12)
  if kind==4:expected[20:32]=o.mem_read(p+0x68,12)
  if kind==3:expected[32:44]=o.mem_read(p+0x74,12)
  assert bytes(x.mem_read(B+44+j*76,76))==expected
 o.mem_write(STACK,w(STOP,OUT,OUT+1,OUT+2,1,B+4,B+16,0x40000000));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x4daff0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert got==bytes(o.mem_read(OUT,3)),(i,got.hex(),bytes(o.mem_read(OUT,3)).hex())
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-transformed-lighting'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,scope='Complete4d9fd0/4d8480 source transforms followed by4daff0/4da8b0 shading, all types/profiles and0-16 sources. All transformed source fields and RGB match. Supplied active table, origin/basis and ambient; no hooks or native XEMU.')
(root/'artifacts/vfx-transformed-lighting.json').write_text(json.dumps(report,indent=2));print(report)

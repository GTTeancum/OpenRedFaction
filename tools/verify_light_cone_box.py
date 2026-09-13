"""Prepared spotlight plane/AABB test against actual original and both builds."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;OUT=B+0x2000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_visibility_light_cone_box\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(entry,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4d81d0);commands=[];expected=[];hits=0
for case in range(4096):
 lo=[rng.randint(-1000,1000)/32 for _ in range(3)];hi=[v+rng.randint(0,100)/32 for v in lo];planes=[]
 for i in range(6):
  normal=[rng.randint(-100,100)/16 for _ in range(3)];distance=rng.randint(-1000,1000)/16
  if case%4==0:
   normal=[0,0,0];distance=-1
   if i==case%6:distance=struct.unpack('<f',w(0xba83126f+(case//4)%3))[0]
  planes.append(f(*normal,distance)+w((case+i)%8))
 data=f(*lo,*hi)+b''.join(planes);commands.append(data);o.mem_write(B,data);o.mem_write(OWNER,bytes(268))
 for i,plane in enumerate(planes):o.mem_write(OWNER+0x94+i*16,plane[:16]);o.mem_write(OWNER+0xf4+i*4,plane[16:])
 hit=call(o,0x4d81d0,[OWNER,B,B+12])&255;hits+=hit;result=w(0,hit);expected.append(result)
 x.mem_write(B,data);x.mem_write(OUT,w(0xa5a5a5a5));status=call(x,entry,[B+24,B,B+12,OUT])
 assert w(status)+bytes(x.mem_read(OUT,4))==result,case
# Invalid selectors, nonfinite plane and reversed box preserve output.
for offset,value in [(40,w(8)),(24,f(float('nan'))),(0,f(100000))]:
 bad=bytearray(data);bad[offset:offset+4]=value;commands.append(bytes(bad));x.mem_write(B,bytes(bad));x.mem_write(OUT,w(0xa5a5a5a5))
 status=call(x,entry,[B+24,B,B+12,OUT]);assert status and bytes(x.mem_read(OUT,4))==w(0xa5a5a5a5);expected.append(w(status,0xa5a5a5a5))
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-cone-box'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=4096,hits=hits,guards=3,original_sha256=sha,scope='Actual 4d81d0 with original corner selection and signed-distance callees. Six supplied planes, all eight selectors, float neighbors around double -0.001 threshold. Plane generation, room tree traversal and dirty-state dispatch remain excluded.')
(root/'artifacts/light-cone-box.json').write_text(json.dumps(report,indent=2));print(report)

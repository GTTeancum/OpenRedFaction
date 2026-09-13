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
mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_visibility_light_cone_prepare\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(entry,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
P=0xc4e7d8;commands=[];expected=[];negative=0
records=[r for level in json.loads((root/'artifacts/level-lights.json').read_text())['results'] for r in level['records'] if (r['flags']>>4)&15==2]
f32=lambda v:struct.unpack('<f',f(v))[0]
rng=random.Random(0x4d9520)
inputs=[(r['position'],r['orientation_disk'][:3],r['radius'],f32((r['inner_angle']+r['outer_delta'])*f32(0.01745329238474369))) for r in records]
inputs += [([rng.uniform(-10,10) for _ in range(3)],[rng.uniform(-1,1) for _ in range(3)],rng.uniform(.1,100),rng.uniform(.01,6.2)) for _ in range(1024)]
for case,(position,axis,radius,angle) in enumerate(inputs):
 data=f(*position,*axis,radius,angle);commands.append(data)
 o.mem_write(P,bytes(268));o.mem_write(P+8,w(3));o.mem_write(P+12,data[:12]);o.mem_write(P+0x24,data[12:24]);o.mem_write(P+0x3c,data[24:28]);o.mem_write(0x1818b84,w(0))
 call(o,0x4d9520,[0,0,struct.unpack('<I',data[28:32])[0]])
 width=struct.unpack('<f',o.mem_read(P+0x8c,4))[0];negative+=width<0
 o.mem_write(STACK,w(STOP,P,0,0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x4d86d0,0x4d89f8,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4d89f8
 planes=b''.join(bytes(o.mem_read(P+0x94+i*16,16))+bytes(o.mem_read(P+0xf4+i*4,4)) for i in range(6));expected.append(w(0)+planes)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*120)
 status=call(x,entry,[B,B+12,struct.unpack('<I',data[24:28])[0],struct.unpack('<I',data[28:32])[0],OUT])
 got=bytes(x.mem_read(OUT,120));assert status==0 and got==planes,(case,angle,width,status,[(i,got[i:i+4].hex(),planes[i:i+4].hex()) for i in range(0,120,4) if got[i:i+4]!=planes[i:i+4]])
for offset,value in [(28,f(0)),(28,f(float('nan'))),(24,f(-1))]:
 bad=bytearray(data);bad[offset:offset+4]=value;commands.append(bytes(bad));x.mem_write(B,bytes(bad));x.mem_write(OUT,b'\xa5'*120)
 status=call(x,entry,[B,B+12,struct.unpack('<I',bad[24:28])[0],struct.unpack('<I',bad[28:32])[0],OUT]);assert status and bytes(x.mem_read(OUT,120))==b'\xa5'*120
 expected.append(w(status)+b'\xa5'*120)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-cone-prepare'],input=b''.join(commands));assert pc==b''.join(expected),next(i for i in range(len(expected)) if pc[i*124:(i+1)*124]!=expected[i])
report=dict(result='PASS',authored_spotlights=len(records),synthetic_cases=1024,guards=3,negative_widths=negative,scope='Actual original4d9520 width generation and4d86d0 plane preparation with all math callees unchanged, versus PC/NXDK. All installed spotlight settings, including180 degrees, plus synthetic acute/obtuse cones. No room traversal or alternate-view dispatch.')
(root/'artifacts/light-cone-prepare.json').write_text(json.dumps(report,indent=2));print(report)

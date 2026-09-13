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
mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_visibility_light_cone_planes\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(entry,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4d86d0);commands=[];expected=[];box_commands=[];box_expected=[]
box_entry=int(re.search(r"\s_rf_visibility_light_cone_box\s+([0-9a-fA-F]+)",mp)[1],16)
for case in range(2048):
 position=[rng.randint(-1000,1000)/32 for _ in range(3)]
 axis=[rng.randint(-100,100)/64 for _ in range(3)]
 if case%8==0:axis=[0,1 if case%16 else -1,0]
 if case%8==1:axis=[.000099,1,-.000099]
 radius=rng.randint(1,1000)/32;width=rng.randint(1,1000)/64
 data=f(*position,*axis,radius,width);commands.append(data)
 o.mem_write(OWNER,bytes(268));o.mem_write(OWNER+8,w(3));o.mem_write(OWNER+12,data[:12]);o.mem_write(OWNER+0x24,data[12:24]);o.mem_write(OWNER+0x3c,data[24:28]);o.mem_write(OWNER+0x8c,data[28:32]);o.mem_write(0x1818b84,w(0))
 o.mem_write(STACK,w(STOP,OWNER,0,0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x4d86d0,0x4d89f8,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4d89f8
 planes=b''.join(bytes(o.mem_read(OWNER+0x94+i*16,16))+bytes(o.mem_read(OWNER+0xf4+i*4,4)) for i in range(6));expected.append(w(0)+planes)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*120)
 status=call(x,entry,[B,B+12,struct.unpack('<I',data[24:28])[0],struct.unpack('<I',data[28:32])[0],OUT])
 got=bytes(x.mem_read(OUT,120));assert status==0 and got==planes,(case,[(i,got[i:i+4].hex(),planes[i:i+4].hex()) for i in range(0,120,4) if got[i:i+4]!=planes[i:i+4]])
 bounds=f(*(v-2 for v in position),*(v+2 for v in position));o.mem_write(B,bounds)
 hit=call(o,0x4d81d0,[OWNER,B,B+12])&255;box_commands.append(bounds+planes);box_expected.append(w(0,hit))
 x.mem_write(B,bounds);x.mem_write(OUT+120,w(0xa5a5a5a5))
 assert call(x,box_entry,[OUT,B,B+12,OUT+120])==0 and bytes(x.mem_read(OUT+120,4))==w(hit)
for offset,value in [(0,f(float('nan'))),(24,f(0)),(28,f(0))]:
 bad=bytearray(data);bad[offset:offset+4]=value;commands.append(bytes(bad));x.mem_write(B,bytes(bad));x.mem_write(OUT,b'\xa5'*120)
 status=call(x,entry,[B,B+12,struct.unpack('<I',bad[24:28])[0],struct.unpack('<I',bad[28:32])[0],OUT]);assert status and bytes(x.mem_read(OUT,120))==b'\xa5'*120
 expected.append(w(status)+b'\xa5'*120)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-cone-planes'],input=b''.join(commands))==b''.join(expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-cone-box'],input=b''.join(box_commands))==b''.join(box_expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,composed_box_cases=2048,guards=3,scope='Original 4d86d0 entry through plane preparation, stopping before room traversal; all math callees execute unchanged. Supplied stored half-width, world position/axis including near-vertical and nonunit directions. Exact six planes and selectors. Width generation, alternate-view transform and dirty-state traversal excluded.')
(root/'artifacts/light-cone-planes.json').write_text(json.dumps(report,indent=2));print(report)

"""Replay full original5085c0 shadow ray/plane intersection against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_ray\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x5085c0);inputs=[];responses=[];hits=parallel=behind=far=0
for i in range(4096):
 start=[rng.uniform(-100,100) for _ in range(3)];direction=[rng.uniform(-2,2) for _ in range(3)];plane=[rng.uniform(-2,2) for _ in range(4)]
 if i%8==0:plane=[0,0,0,1]
 if i%8==1:plane=[1,0,0,-start[0]];direction=[0,1,0]
 if i%8==2:plane=[1,0,0,-start[0]]
 if i%8==3:plane=[1,0,0,-start[0]-100];direction=[.01,0,0]
 if i%8==4:plane=[1,0,0,-start[0]+100];direction=[.01,0,0]
 data=f(*start,*direction,*plane);o.mem_write(B,data);o.mem_write(OUT,bytes([165])*12)
 hit=call(o,0x5085c0,[B,B+24,OUT])&255;assert hit in (0,1);expected=bytes(o.mem_read(OUT,12))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*12);x.mem_write(OWNER,w(0xa5a5a5a5));assert call(x,entry,[B,B+12,B+24,OUT,OWNER])==0
 got_hit=struct.unpack('<I',x.mem_read(OWNER,4))[0];got=bytes(x.mem_read(OUT,12));assert got_hit==hit and got==expected,(i,hit,got_hit,got.hex(),expected.hex())
 inputs.append(data);responses.append(w(0,hit)+expected);hits+=hit
 if expected==bytes([165])*12:parallel+=1
 elif not hit:behind+=1
 if i%8==3:assert hit;far+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-ray'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,f(float('nan'))),(12,f(float('inf'))),(24,f(float('nan'))),(36,f(float('inf')))]:
 bad=bytearray(data);bad[at:at+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*12);x.mem_write(OWNER,w(0xa5a5a5a5))
 status=call(x,entry,[B,B+12,B+24,OUT,OWNER]);assert status!=0 and bytes(x.mem_read(OUT,12))==bytes([165])*12 and bytes(x.mem_read(OWNER,4))==w(0xa5a5a5a5)
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-ray'],input=bytes(bad))==w(status,0xa5a5a5a5)+bytes([165])*12
report=dict(result='PASS',original_pc_nxdk_rays=len(inputs),hits=hits,parallel=parallel,behind_origin=behind,explicit_beyond_segment_hits=far,pc_nxdk_guards=4,original_sha256=sha,x87_control_word='0x027f',scope='Full unhooked5085c0 and actual dot/plane/vector helpers, including parallel output preservation, negative-parameter output writes, on-plane origins and positive intersections beyond parameter1. Compiled NXDK CPU replay; shadow volume construction and native rendered integration excluded.')
(root/'artifacts/lightmap-shadow-ray.json').write_text(json.dumps(report,indent=2));print(report)

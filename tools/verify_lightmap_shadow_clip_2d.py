"""Replay full original549f10 projected polygon clipping against PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_clip_2d\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x549f10);inputs=[];responses=[];empty=unchanged=clipped=0
for i in range(2048):
 nb=3+i%6;ns=3+(i//6)%6;cx=rng.uniform(-5,5);cy=rng.uniform(-5,5);rx=rng.uniform(.5,8);ry=rng.uniform(.5,8)
 boundary=[(rx*math.cos(j*2*math.pi/nb),ry*math.sin(j*2*math.pi/nb)) for j in range(nb)]
 subject=[(cx+4*math.cos(j*2*math.pi/ns),cy+4*math.sin(j*2*math.pi/ns)) for j in range(ns)]
 if i%2:boundary.reverse()
 if i%3==0:subject.reverse()
 if i%7==0:
  nb=4;ns=4;boundary=[(0,0),(1,0),(1,1),(0,1)];d=[0,.000099,.0001,.000101,-.0001,-.000101][(i//7)%6];subject=[(d,.25),(.75,.25),(.75,.75),(d,.75)]
 if i%11==0:subject=boundary[:];ns=nb
 rawb=b''.join(f(*v) for v in boundary).ljust(128,b'\0');raws=b''.join(f(*v) for v in subject).ljust(128,b'\0');data=w(nb,ns,64)+rawb+raws
 o.mem_write(B,data);o.mem_write(OUT,bytes([165])*512);o.mem_write(OWNER,w(0));n=call(o,0x549f10,[nb,B+12,ns,B+140,OUT,OWNER]);assert n<=30
 ptr=struct.unpack('<I',o.mem_read(OWNER,4))[0];expected=(bytes(o.mem_read(ptr,n*8)) if n else b'')+bytes([165])*(512-n*8)
 if not n:empty+=1
 elif ptr==B+140:unchanged+=1
 else:clipped+=1
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*512);x.mem_write(OWNER,w(0xa5a5a5a5));x.mem_write(B+0x6000,w(B+0x8000,B+0x9000,B+0xa000,64))
 status=call(x,entry,[B+12,nb,B+140,ns,B+0x6000,OUT,64,OWNER]);got_n=struct.unpack('<I',x.mem_read(OWNER,4))[0];got=bytes(x.mem_read(OUT,512));assert status==0 and got_n==n and got==expected,(i,status,got_n,n,got[:got_n*8].hex(),expected[:n*8].hex())
 inputs.append(data);responses.append(w(0,n)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-clip-2d'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,w(2)),(4,w(2)),(12,f(float('nan'))),(140,f(float('inf')))]:
 bad=bytearray(data);bad[at:at+4]=value;nb,ns,cap=struct.unpack('<III',bad[:12]);x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*512);x.mem_write(OWNER,w(0xa5a5a5a5))
 status=call(x,entry,[B+12,nb,B+140,ns,B+0x6000,OUT,cap,OWNER]);assert status!=0 and bytes(x.mem_read(OUT,512))==bytes([165])*512 and bytes(x.mem_read(OWNER,4))==w(0xa5a5a5a5)
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-clip-2d'],input=bytes(bad))==w(status,0xa5a5a5a5)+bytes([165])*512
# Output and scratch shortages must not publish a partial polygon.
rect=b''.join(f(*v) for v in [(0,0),(4,0),(4,4),(0,4)]).ljust(128,b'\0')
bad=w(4,4,3)+rect+rect;x.mem_write(B,bad);x.mem_write(OUT,bytes([165])*512);x.mem_write(OWNER,w(0xa5a5a5a5))
status=call(x,entry,[B+12,4,B+140,4,B+0x6000,OUT,3,OWNER]);assert status!=0 and bytes(x.mem_read(OUT,512))==bytes([165])*512 and bytes(x.mem_read(OWNER,4))==w(0xa5a5a5a5)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-clip-2d'],input=bad)==w(status,0xa5a5a5a5)+bytes([165])*512
x.mem_write(B+0x600c,w(3));assert call(x,entry,[B+12,4,B+140,4,B+0x6000,OUT,64,OWNER])!=0 and bytes(x.mem_read(OUT,512))==bytes([165])*512 and bytes(x.mem_read(OWNER,4))==w(0xa5a5a5a5)
report=dict(result='PASS',original_pc_nxdk_polygons=len(inputs),empty=empty,unchanged=unchanged,clipped=clipped,pc_nxdk_guards=5,nxdk_scratch_capacity_guards=1,original_sha256=sha,x87_control_word='0x027f',scope='Full unhooked549f10 and actual2D dot/copy helpers. Convex boundaries in both winding orders, exact boundaries and epsilon neighbors, rejected/unchanged/clipped results. Count and ordered output float bits exact; caller-owned scratch replaces original globals. Area filtering, occluder ownership and native rendered shadows excluded.')
(root/'artifacts/lightmap-shadow-clip-2d.json').write_text(json.dumps(report,indent=2));print(report)

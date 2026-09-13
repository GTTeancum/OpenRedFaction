"""Full unhooked4f2100 polygon mask rasterization vs PC and compiled NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VERT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_raster_shadow\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f2100);inputs=[];responses=[];changed=0;guard_writes=0
for i in range(2048):
 width=1+i%24;height=1+(i//24)%24;n=3+i%14;cx=rng.uniform(-3,width+3);cy=rng.uniform(-3,height+3);rx=rng.uniform(.1,width+4);ry=rng.uniform(.1,height+4)
 if i%4==0:
  n=4;v=[(cx-rx,cy-ry),(cx+rx,cy-ry),(cx+rx,cy+ry),(cx-rx,cy+ry)]
 else: v=[(cx+rx*math.cos(j*2*math.pi/n),cy+ry*math.sin(j*2*math.pi/n)) for j in range(n)]
 if i%7==0:v=[(round(a*2)/2,round(b*2)/2) for a,b in v]
 if i%2:v.reverse()
 rotate=i%n;v=v[rotate:]+v[:rotate];vertices=b''.join(f(*q) for q in v).ljust(128,b'\0');amount=[0,1,127,255][(i//4)%4];initial=bytes(rng.randrange(256) for _ in range(1024));capacity=width*(height+1)+1
 o.mem_write(VERT,vertices);o.mem_write(OUT,initial);call(o,0x4f2100,[0,n,VERT,OUT,width,height,amount]);expected=bytes(o.mem_read(OUT,1024));assert expected[capacity:]==initial[capacity:]
 x.mem_write(VERT,vertices);x.mem_write(OUT,initial);assert call(x,entry,[VERT,n,OUT,capacity,width,height,amount])==0
 got=bytes(x.mem_read(OUT,1024));assert got==expected,(i,v,[(j,a,b) for j,(a,b) in enumerate(zip(got,expected)) if a!=b][:8])
 inputs.append(w(n,width,height,capacity,amount)+vertices+initial);responses.append(w(0)+expected);changed+=sum(a!=b for a,b in zip(initial,expected));guard_writes+=sum(a!=b for a,b in zip(initial[width*height:capacity],expected[width*height:capacity]))
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-raster-shadow'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,w(0)),(4,w(0)),(8,w(0xffffffff)),(12,w(capacity-1)),(20,f(float('nan'))),(24,f(float('inf')))]:
 data=bytearray(inputs[-1]);data[at:at+4]=value;n,ww,hh,cap,amt=struct.unpack('<5I',data[:20]);x.mem_write(VERT,bytes(data[20:148]));x.mem_write(OUT,initial)
 status=call(x,entry,[VERT,n,OUT,cap,ww,hh,amt]);assert status!=0 and bytes(x.mem_read(OUT,1024))==initial
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-raster-shadow'],input=bytes(data))==w(status)+initial
report=dict(result='PASS',original_pc_nxdk_polygons=len(inputs),changed_bytes=changed,guard_row_changed_bytes=guard_writes,pc_nxdk_guards=6,original_sha256=sha,x87_control_word='0x027f',scope='Full original4f2100 with actual ceil/floor/ftol and vector/min/max helpers, no hooks. Reversed/rotated convex polygons, rectangles, half-pixel ties, clipped bounds, wrapping subtraction and explicit guard row. Compiled NXDK CPU replay; projection, mask allocation, live binding and native XEMU visuals excluded.')
(root/'artifacts/lightmap-raster-shadow.json').write_text(json.dumps(report,indent=2));print(report)

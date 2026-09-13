"""Original special lightmap two-edge interpolation vs PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_interpolate_edges\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)

NODES=B+0x6000
rng=random.Random(0x4f39dd);inputs=[];responses=[]
for i in range(4096):
 vertices=[]
 for j in range(4):vertices.append([rng.uniform(-2,2),(-1 if j%2==0 else 1)*rng.uniform(.1,2)]+[rng.uniform(-100,100) for k in range(3)]+[rng.uniform(-1,1) for k in range(3)])
 if i%8==0:
  for j in range(4):vertices[j][0]=.25
 if i%8==1:
  for j in range(4):vertices[j][0]=.25+(2**-23 if j>=2 else 0)
 center=[rng.uniform(-2,2),rng.uniform(-3,3)]
 data=b''.join(f(*v) for v in vertices)+f(*center);inputs.append(data)
 o.mem_write(B,data);o.mem_write(STACK,bytes(1024))
 for j in range(4):
  node=NODES+j*24;o.mem_write(node,w(B+j*32+8)+bytes(8)+data[j*32:j*32+8]+w(0))
  o.mem_write(STACK+0xa0+j*8,w(node,B+j*32+20))
 o.mem_write(STACK+0x18,data[128:132]);o.mem_write(STACK+0x24,data[132:136]);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f)
 o.emu_start(0x4f39dd,0x4f3d29,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4f3d29
 expected=bytes(o.mem_read(STACK+0xf8,12))+bytes(o.mem_read(STACK+0xe0,12))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*24);assert call(x,entry,[B,B+128,OUT])==0
 got=bytes(x.mem_read(OUT,24));assert got==expected,(i,got.hex(),expected.hex());responses.append(w(0)+expected)
probe=[str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-interpolate-edges']
got=subprocess.check_output(probe,input=b''.join(inputs));assert got==b''.join(responses),next((i for i in range(len(inputs)) if got[i*28:i*28+28]!=responses[i]),-1)
guards=[]
bad=bytearray(inputs[0]);bad[36:40]=bad[4:8];guards.append(bytes(bad))
bad=bytearray(inputs[0]);bad[128:132]=f(math.nan);guards.append(bytes(bad))
for data in guards:
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*24);status=call(x,entry,[B,B+128,OUT]);assert status!=0 and bytes(x.mem_read(OUT,24))==bytes([165])*24
 assert subprocess.check_output(probe,input=data)==w(status)+bytes([165])*24
report=dict(result='PASS',original_pc_nxdk_samples=len(inputs),zero_horizontal_span_cases=512,near_zero_span_cases=512,guards=len(guards),original_sha256=sha,x87_control_word='0x027f',scope='Original4f39dd..4f3d24 with actual vector subtract/multiply/add/copy helpers and no hooks. Both position and unnormalized normal match byte-for-byte; unclamped factors and zero-span substitution included. Edge selection and live lighting resources excluded.')
(root/'artifacts/lightmap-interpolate-edges.json').write_text(json.dumps(report,indent=2));print(report)

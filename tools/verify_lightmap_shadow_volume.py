"""Replay original shadow-volume plane construction against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EDI
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_volume\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f4c03);inputs=[];responses=[]
for i in range(2048):
 normal=[rng.uniform(-1,1) for _ in range(3)];center=[rng.uniform(-30,30) for _ in range(3)];origin=[rng.uniform(-30,30) for _ in range(3)];corners=[[rng.uniform(-30,30) for _ in range(3)] for _ in range(4)]
 plane=normal+[-sum(a*b for a,b in zip(normal,center))]
 if i%2==0:
  axis=i%3;normal=[0,0,0];normal[axis]=1;plane=normal+[-center[axis]];origin=center[:];origin[axis]+=rng.uniform(1,50);u=(axis+1)%3;v=(axis+2)%3
  for j,(dx,dy) in enumerate([(-1,-1),(1,-1),(1,1),(-1,1)]):
   corners[j]=center[:];corners[j][u]+=dx*8;corners[j][v]+=dy*6
 if i%4==0:corners[1]=corners[0][:]
 if i%8==0:corners=[origin[:] for _ in range(4)]
 data=f(*plane,*origin,*center,*[q for p in corners for q in p]);o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+0x6c,f(*plane));o.mem_write(STACK,bytes(4096));o.mem_write(STACK+0x64,f(*origin));o.mem_write(STACK+0xc8,f(*center));o.mem_write(STACK+0x8c,f(*[q for p in corners for q in p]))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EDI,OWNER);o.reg_write(UC_X86_REG_ESI,OWNER+0x6c);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f4c03,0x4f4daa,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f4daa;expected=bytes(o.mem_read(STACK+0x1a0,96))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*96);assert call(x,entry,[B,B+16,B+28,B+40,OUT])==0
 got=bytes(x.mem_read(OUT,96));assert got==expected,(i,got.hex(),expected.hex());inputs.append(data);responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-volume'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,f(float('nan'))),(16,f(float('inf'))),(28,f(float('nan'))),(40,f(float("inf")))]:
 bad=bytearray(data);bad[at:at+len(value)]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*96)
 status=call(x,entry,[B,B+16,B+28,B+40,OUT]);assert status!=0 and bytes(x.mem_read(OUT,96))==bytes([165])*96
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-volume'],input=bytes(bad))==w(status)+bytes([165])*96
report=dict(result='PASS',original_pc_nxdk_volumes=len(inputs),planes=len(inputs)*6,collapsed_edge_cases=512,all_corners_at_origin=256,pc_nxdk_guards=4,original_sha256=sha,x87_control_word='0x027f',scope='Original4f4c03..4f4daa with actual plane constructors, three-point normalization, orientation loop and final receiver-plane replacement. Collapsed edges, coincident origin/corners, axis-aligned and arbitrary supplied corners/normals/origins; exact96-byte outputs. Reuses reconstructed visibility plane math. Facing test, UV corner setup, face culling, complete masks and native rendering excluded.')
(root/'artifacts/lightmap-shadow-volume.json').write_text(json.dumps(report,indent=2));print(report)

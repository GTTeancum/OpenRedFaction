"""Full original4e1ad0 UV fan interpolation versus shared PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0x30000000,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_collision_texture_coordinates\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
B=0x30000000;VERT=B+0x1000;EDGE=B+0x2000;POINT=B+0x3000;OUT=B+0x4000;S=B+0xe000;STOP=B+0xf000
branches={0x4e1b7b:0,0x4e1bed:0,0x4e1c93:0}
def observe(cpu,address,size,context):branches[address]+=1
for a in branches:u.hook_add(UC_HOOK_CODE,observe,begin=a,end=a)
def run(cpu,fn,args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.reg_write(UC_X86_REG_FPCW,0x27f if cpu is x else 0x37f);cpu.emu_start(fn,STOP,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;assert cpu.reg_read(UC_X86_REG_FPCW)==(0x27f if cpu is x else 0x37f);return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4e1ad0);commands=[];answers=[];hits=0
normals=((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1),(1,1,1),(-1,1,1),(1,1,0),(0,1,1),(0,0,0))
for case in range(8192):
 count=3+case%6;normal=normals[case%len(normals)];a=list(map(abs,normal));axis=(0 if a[0]>a[2] else 2) if a[0]>a[1] else (1 if a[1]>a[2] else 2);axes=((2,1),(0,2),(1,0))[axis];p,q=axes if normal[axis]>0 else axes[::-1]
 vertices=[[0.,0.,0.] for _ in range(8)];coordinates=[[rng.uniform(-8,8),rng.uniform(-8,8)] for _ in range(8)];height=rng.uniform(-4,4)
 for i in range(count):
  theta=2*math.pi*i/count;vertices[i][p]=4*math.cos(theta);vertices[i][q]=3*math.sin(theta);vertices[i][axis]=height
 if case%4==0:
  for i in range(count):vertices[i][p]=rng.uniform(-4,4);vertices[i][q]=rng.uniform(-3,3)
 if case%4==1:vertices[1][p]=vertices[0][p]+(0,1e-5,-1e-5,1e-4,-1e-4)[case//4%5]
 if case%31==0:
  for i in range(count):vertices[i]=vertices[0].copy()
 point=[0.,0.,0.];point[p]=rng.uniform(-5,5);point[q]=rng.uniform(-4,4);point[axis]=height+rng.choice((0,0,10))
 if case%7==0:point=vertices[case%count].copy()
 elif case%7==1:point=[(vertices[0][j]+vertices[1][j])/2 for j in range(3)]
 wire=f(*normal,*point,*[v for row in vertices for v in row],*[v for row in coordinates for v in row])+w(count);commands.append(wire)
 u.mem_write(B,wire[:12]+bytes(64));u.mem_write(B+0x40,w(EDGE));u.mem_write(VERT,wire[24:120]);u.mem_write(POINT,wire[12:24]);u.mem_write(OUT,b'\xa5'*8)
 for i in range(count):u.mem_write(EDGE+i*32,w(VERT+i*12)+wire[120+i*8:128+i*8]+bytes(8)+w(EDGE+(i+1)%count*32,EDGE+(i-1)%count*32)+bytes(4))
 before=bytes(u.mem_read(EDGE,count*32));u.reg_write(UC_X86_REG_ECX,B);matched=run(u,0x4e1ad0,(POINT,OUT,OUT+4))&255;assert matched in (0,1);hits+=matched
 want=w(0,matched)+bytes(u.mem_read(OUT,8));assert bytes(u.mem_read(EDGE,count*32))==before;answers.append(want)
 x.mem_write(B,wire);x.mem_write(OUT,b'\xa5'*12);status=run(x,entry,(B,B+12,B+24,B+120,count,OUT,OUT+8));actual=w(status)+bytes(x.mem_read(OUT+8,4))+bytes(x.mem_read(OUT,8))
 assert actual==want,('NXDK',case,actual.hex(),want.hex())
for offset,value,status in ((0,math.nan,-2),(12,math.inf,-2),(24,math.nan,-2),(120,math.inf,-2),(184,2,-4),(184,9,-4)):
 wire=bytearray(commands[0]);struct.pack_into('<I' if offset==184 else '<f',wire,offset,value);commands.append(bytes(wire));answers.append(w(status)+b'\xa5'*12)
 # count9 is a probe storage bound; use65537 to exercise the production bound.
 count=65537 if value==9 and offset==184 else struct.unpack_from('<I',wire,184)[0]
 x.mem_write(B,bytes(wire));x.mem_write(OUT,b'\xa5'*12);actual=w(run(x,entry,(B,B+12,B+24,B+120,count,OUT,OUT+8)))+bytes(x.mem_read(OUT+8,4))+bytes(x.mem_read(OUT,8));assert actual==answers[-1]
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--texture-uv'],input=b''.join(commands));assert pc==b''.join(answers),'PC mismatch'
assert 0<hits<8192 and all(branches.values())
report=dict(result='PASS',original_cases=8192,port_guards=6,hits=hits,branches={hex(k):v for k,v in branches.items()},original_sha256=sha,scope='Full original4e1ad0 plus4fa6d0 unchanged, no supplied callees. Exact PC/NXDK UV and first-triangle result; dominant-axis ties/signs, 3..8 corners, irregular/degenerate fans, vertex/edge/outside points, strict near-zero branch and noncoplanar projection. Miss/error output preservation and finite/count guards. Original x87 control37f, compiled NXDK control27f preserved; authored face binding and bitmap sampling remain separate.')
(root/'artifacts/collision-texture-uv.json').write_text(json.dumps(report,indent=2));print(report)

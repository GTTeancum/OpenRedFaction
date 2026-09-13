"""Full4077a0 with actual weapon recursion, seat selection, scalar and maximum."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x10000);ACTORS=[B+i*0x2000 for i in range(4)];STACK=B+0xe000;STOP=B+0xf000;STUB=STOP+0x100;OUT=B+0xd000
u.mem_write(STUB,b'\xd9\x1d'+w(OUT)+b'\x83\xc4\x04\xc3')
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
wire=[];selected=[];lookups=[]
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x426fc0:
  h=r(cpu,sp+4);lookups.append(h);i=h-10;value=ACTORS[i] if 0<=i<4 and wire[i*9+8]==1 else 0
  cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
 elif address==0x401cc0:selected.append((ACTORS.index(r(cpu,sp+4)-0x2a0),r(cpu,sp+8)&255))
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x4077a0);commands=[];expected=[];advances=0;lookup_count=0
values=[0,0x80000000,0x3f000000,0x3f800000,0xbf800000,0x7fc12345,0x7f800000,0xff800000]
for case in range(2048):
 wire=[]
 for i in range(4):
  successor=11+i if i<3 else -1;seats=rng.choice(([-1,-1,successor],[-1,99,successor],[successor,-1,99],[-1,-1,-1]))
  weapons=[rng.choice((-1,-1,0,1,2,63)) for _ in range(2)]
  if case%4==0:weapons=[-1,-1];seats=[-1,-1,successor]
  wire += [rng.choice((0,0,0x80000000,0x3f800000)),*weapons,rng.choice((0,1,2,256,257)),rng.choice(values),*seats,rng.choice((0,1,1,1,2))]
 wire += [rng.choice(values) for _ in range(64)];snapshots=[]
 for i,a in enumerate(ACTORS):
  u.mem_write(a,bytes(0x1500));u.mem_write(a+0x2a0,w(a,*wire[i*9+1:i*9+3]));u.mem_write(a+0x294,w(a+0x1000));u.mem_write(a+0x1050,w(wire[i*9]));u.mem_write(a+0x7c8,bytes([wire[i*9+3]&255]));u.mem_write(a+0x7cc,w(wire[i*9+4]));u.mem_write(a+0x8cc,w(3,3,a+0x1100));u.mem_write(a+0x1100,w(a+0x1200,a+0x1210,a+0x1220))
  for j in range(3):u.mem_write(a+0x1200+j*16,w(0,wire[i*9+5+j]))
  snapshots.append(bytes(u.mem_read(a,0x1500)))
 for i,v in enumerate(wire[36:]):u.mem_write(0x85d21c+i*0x550,w(v))
 u.mem_write(STACK,w(STUB,B+0x2a0,STOP));u.reg_write(UC_X86_REG_ESP,STACK);selected=[];lookups=[];u.emu_start(0x4077a0,STOP,count=200000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and len(selected)==2 and selected[0][0]==selected[1][0] and [a[1] for a in selected]==[0,1]
 assert snapshots==[bytes(u.mem_read(a,0x1500)) for a in ACTORS]
 selected_index=selected[0][0];advances+=int(selected_index>0);lookup_count+=len(lookups)
 commands.append(w(*wire));expected.append(w(0,r(u,OUT),selected_index))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-route-limit'],input=b''.join(commands))
for i,e in enumerate(expected):assert actual[i*12:(i+1)*12]==e,('PC',i,actual[i*12:(i+1)*12].hex(),e.hex())
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im);x.mem_map(B,0x10000)
entry=int(re.search(r'\s_rf_entity_ai_route_limit\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);REG=B+0x8000;TABLE=B+0x9000;CB=STOP+0x200;VIEWS=[B+0x100*i for i in range(4)];which=0xffffffff;failure=False

def xhook(cpu,address,size,context):
 global which
 if address!=CB:return
 sp=cpu.reg_read(UC_X86_REG_ESP);i=VIEWS.index(r(cpu,sp+8));which=i
 if not failure:cpu.mem_write(r(cpu,sp+12),w(wire[i*9+3]));cpu.mem_write(r(cpu,sp+16),w(wire[i*9+4]))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if failure else 0);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,xhook)
def setup(command):
 global wire,which
 wire=list(struct.unpack('<100I',command));which=0xffffffff;x.mem_write(REG,bytes(4096));x.mem_write(TABLE,w(*wire[36:]));x.mem_write(OUT,w(0x12345678))
 for i,a in enumerate(VIEWS):
  seats=B+0x1000+i*16;x.mem_write(seats,w(*wire[i*9+5:i*9+8]));x.mem_write(a,w(10+i,1 if wire[i*9+8]==2 else 0,0,0,0,0,0,0,*wire[i*9+1:i*9+3],wire[i*9],a,seats,3))
  if wire[i*9+8]:x.mem_write(REG+4*(10+i),w(a))
 x.mem_write(STACK,w(STOP,REG,B,TABLE,64,CB,0,OUT));x.reg_write(UC_X86_REG_ESP,STACK)
def run():
 x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
for i,command in enumerate(commands):
 setup(command);assert w(run(),r(x,OUT),which)==expected[i],('NXDK',i)
setup(commands[0]);failure=True;assert run()==0xffffffff and r(x,OUT)==0x12345678;failure=False
# Original cycles do not terminate. Shared stable-view guard rejects without override access.
setup(commands[0]);x.mem_write(B+4,w(0));x.mem_write(B+32,w(-1,-1,0));x.mem_write(B+0x1000,w(10,-1,-1));x.mem_write(REG+40,w(B));assert run()==0xfffffffe and which==0xffffffff and r(x,OUT)==0x12345678
report=dict(result='PASS',original_pc_nxdk_cases=2048,advanced_inventory_cases=advances,original_lookup_calls=lookup_count,compiled_error_guards=2,scope='Full original4077a0 with actual408dc0 recursion,40a2a0,427da0/list helpers,401cc0 and40a4a0. Only typed lookup supplied. PC/NXDK exact result bits and final inventory agree; original source storage unchanged. Shared stable views collapse repeated unsuccessful presence walks. Override source and scalar table supplied, no live scene binding.')
(root/'artifacts/ai-route-limit.json').write_text(json.dumps(report,indent=2));print(report)

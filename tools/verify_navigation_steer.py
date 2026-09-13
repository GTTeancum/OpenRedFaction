"""Complete original40cb50 steering versus shared PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
B=0x30000000;ACTOR=B;CLASS=B+0x2000;MODE=B+0x3000;INPUT=B+0x4000;OUT=B+0x5000;STACK=B+0xe000;STOP=B+0xf000;RETURN=B+0xf100

def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,65536);return m
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_steer\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.mem_write(RETURN,b'\xd9\x1d'+w(OUT)+b'\xe9'+w(STOP-(RETURN+11)));u.mem_write(0x5afb1c,b'\x01');u.mem_write(0x5afb10,f(0,-1,0));u.mem_write(0x20850f4,w(1))
rng=random.Random(0x40cb50);commands=[];expected=[]
for i in range(2048):
 rate=rng.choice((0,-1,.1,1,2,10));vectors=[rng.uniform(-1,1) for _ in range(12)];modes=[rng.choice((0,1,2)) for _ in range(2)];flags=rng.choice((0,0x200,0x400000,0x400200));state=f(rate,*vectors)+w(*modes,flags)+f(*[rng.uniform(-.5,.5) for _ in range(6)])+w(rng.getrandbits(32));target=f(*[rng.uniform(-5,5) for _ in range(3)])
 if i%8==0:target=state[16:28] if 1 in modes else state[4:16]
 if i%8==1:
  origin=list(struct.unpack('<3f',state[16:28] if 1 in modes else state[4:16]));origin[1]+=1;target=f(*origin)
 wire=state+target+f(rng.choice((1/60,1/30,.1)))+w(rng.getrandbits(32));assert len(wire)==112;commands.append(wire)
 u.mem_write(ACTOR,bytes(0x900));u.mem_write(CLASS,bytes(0x800));u.mem_write(MODE,bytes(32));u.mem_write(ACTOR+0x294,w(CLASS));u.mem_write(ACTOR+0x858,w(MODE));u.mem_write(CLASS+0x60,state[:4]);u.mem_write(CLASS+0x724,state[60:64]);u.mem_write(MODE+20,state[52:60])
 for offset,data in ((0xe4,state[4:16]),(0x7d4,state[16:28]),(0x7e0,state[28:40]),(0x7f8,state[40:52]),(0x150,state[64:76]),(0x708,state[76:88]),(0x7b0,state[88:92])):u.mem_write(ACTOR+offset,data)
 u.mem_write(INPUT,wire);u.mem_write(0x5a4014,wire[104:108]);u.mem_write(0x6460f0,wire[108:112]);u.mem_write(STACK,w(RETURN,ACTOR,INPUT+92));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x40cb50,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 after=state[:64]+bytes(u.mem_read(ACTOR+0x150,12))+bytes(u.mem_read(ACTOR+0x708,12))+bytes(u.mem_read(ACTOR+0x7b0,4));want=w(0)+bytes(u.mem_read(OUT,4))+after;expected.append(want)
 x.mem_write(INPUT,wire);x.mem_write(OUT,f(99));x.mem_write(STACK,w(STOP,INPUT,INPUT+92,*struct.unpack('<2I',wire[104:112]),OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,4))+bytes(x.mem_read(INPUT,92));assert got==want,('NXDK',i,got.hex(),want.hex())
# Invalid reached frame durations preserve prepared actor state and result.
seed=next(v for v in commands if struct.unpack_from('<f',v)[0]>0 and v[92:104]!=(v[16:28] if 1 in struct.unpack_from('<2i',v,52) else v[4:16]))
for seconds in (0,0x7fc00000):
 wire=seed[:104]+w(seconds)+seed[108:];x.mem_write(INPUT,wire);x.mem_write(OUT,f(99));x.mem_write(STACK,w(STOP,INPUT,INPUT+92,seconds,struct.unpack_from('<I',wire,108)[0],OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
 want=w(0xfffffffe)+f(99)+wire[:92];got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,4))+bytes(x.mem_read(INPUT,92));assert got==want
 commands.append(wire);expected.append(want)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-steer'],input=b''.join(commands))
for i,want in enumerate(expected):assert actual[i*100:(i+1)*100]==want,('PC',i,actual[i*100:(i+1)*100].hex(),want.hex())
report=dict(result='PASS',original_pc_nxdk_cases=2048,compiled_failure_guards=2,scope='Full unhooked40cb50 with actual vector math, acos and class predicates. Static down-vector/CRT initialized before entry. Exact return/angular/command/clock fields, position-source modes, zero/coincident/vertical targets, angular limiting and publication flags. Finite prepared inputs; no route dispatch or native scene steering.')
(root/'artifacts/navigation-steer.json').write_text(json.dumps(report,indent=2));print(report)

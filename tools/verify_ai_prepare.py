"""Full40aae0 original/PC/NXDK ordering, movement predicates and publication."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x10000);MOV=B+0x2000;STACK=B+0xe000;STOP=B+0xf000
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
wire=[];calls=0
def hook(cpu,address,size,context):
 global calls
 if address!=0x408dc0:return
 calls+=1;sp=cpu.reg_read(UC_X86_REG_ESP);assert r(cpu,sp+4)==B+0x2a0
 assert r(cpu,0x5af630)==wire[0] and r(cpu,0x5af634)==wire[1]
 cpu.mem_write(MOV+4,w(wire[7]));cpu.mem_write(B+0x3c,w(*wire[8:11]));cpu.mem_write(B+0x7c0,w(*wire[11:13]));cpu.mem_write(B+0x69c,w(wire[3]^0x55555555,wire[4]^0xaaaaaaaa))
 cpu.reg_write(UC_X86_REG_EAX,wire[6]);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x40aae0);commands=[];expected=[];specials=0
for case in range(2048):
 wire=[rng.getrandbits(32) for _ in range(13)];wire[0]=0x3f800000;wire[1]=0x40000000;wire[2]=case%20;wire[7]=(case//20)%20 if case%4 else 0xffffffff;wire[6]=rng.choice((0,1,2,255,256,257));wire[8:11]=[0x40800000,0x40a00000,0x40c00000];wire[11:13]=[0x41200000,0x41a00000]
 seed=bytearray(rng.randbytes(0x1500));seed[0x858:0x85c]=w(MOV);seed[0x7c0:0x7c8]=w(*wire[:2]);seed[0x69c:0x6a4]=w(*wire[3:5]);u.mem_write(B,bytes(seed));u.mem_write(MOV+4,w(wire[2]));query=rng.randbytes(0x40);u.mem_write(0x5af618,query);u.mem_write(0x6460e8,w(wire[5]))
 u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK);calls=0;u.emu_start(0x40aae0,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP and calls==1
 special=wire[7] in (12,15,13,11,9,4,7);specials+=special;want=bytearray(seed)
 for off,data in ((0x3c,w(*wire[8:11])),(0x7c0,w(*wire[11:13])),(0x69c,w(wire[3]^0x55555555,wire[4]^0xaaaaaaaa)),(0x5e4,w(special)),(0x5a4,w(*wire[8:11])),(0x5b0,w(*wire[8:11]))):want[off:off+len(data)]=data
 assert bytes(u.mem_read(B,len(seed)))==want
 wantq=bytearray(query)
 for address,value in ((0x5af630,wire[0]),(0x5af634,wire[1]),(0x5af650,B+0x58c),(0x5af618,B+0x5a4),(0x5af640,0),(0x5af61c,wire[3]^0x55555555),(0x5af620,wire[4]^0xaaaaaaaa),(0x5af644,0),(0x5af63c,wire[5])):off=address-0x5af618;wantq[off:off+4]=w(value)
 wantq[0x20]=int(not special);wantq[0x21]=wire[6]&255;assert bytes(u.mem_read(0x5af618,64))==wantq
 commands.append(w(*wire));expected.append(w(0,r(u,B+0x5e4),*struct.unpack('<3I',u.mem_read(B+0x5a4,12)),*struct.unpack('<3I',u.mem_read(B+0x5b0,12)),r(u,0x5af630),r(u,0x5af634),u.mem_read(0x5af639,1)[0],u.mem_read(0x5af638,1)[0],r(u,0x5af618),r(u,0x5af650),r(u,0x5af61c),r(u,0x5af620),r(u,0x5af640),r(u,0x5af644),r(u,0x5af63c)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-prepare'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im);x.mem_map(B,0x10000)
entry=int(re.search(r'\s_rf_entity_ai_destination_prepare\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);Q=B+0x1000;CB=STOP+0x100;failure=False

def xhook(cpu,address,size,context):
 global calls
 if address!=CB:return
 calls+=1;sp=cpu.reg_read(UC_X86_REG_ESP);assert r(cpu,sp+8)==B and r(cpu,Q+32)==wire[0] and r(cpu,Q+12)==wire[1]
 cpu.mem_write(B+136,w(wire[7]));cpu.mem_write(B+24,w(*wire[8:11]));cpu.mem_write(B+16,w(*wire[11:13]));cpu.mem_write(B+144,w(wire[3]^0x55555555,wire[4]^0xaaaaaaaa));cpu.mem_write(r(cpu,sp+12),w(wire[6]))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if failure else 0);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,xhook)
def setup(command):
 global wire,calls
 wire=list(struct.unpack('<13I',command));calls=0;x.mem_write(B,b'Z'*152);x.mem_write(Q,b'Z'*64);x.mem_write(B+16,w(*wire[:2]));x.mem_write(B+136,w(wire[2]));x.mem_write(B+144,w(*wire[3:5]));x.mem_write(STACK,w(STOP,B,Q,wire[5],CB,0));x.reg_write(UC_X86_REG_ESP,STACK)
def run():
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
for i,command in enumerate(commands):
 setup(command);status=run();out=[status,r(x,B+140),*struct.unpack('<3I',x.mem_read(B+72,12)),*struct.unpack('<3I',x.mem_read(B+84,12)),r(x,Q+32),r(x,Q+12),r(x,Q+36),r(x,Q+16),0x300005a4 if r(x,Q+40)==B+72 else 0,0x3000058c if r(x,Q+52)==B+112 else 0,r(x,Q+44),r(x,Q+48),r(x,Q+24),r(x,Q+60),r(x,Q+56)]
 assert w(*out)==expected[i],('NXDK',i);assert x.mem_read(Q,12)==b'Z'*12 and r(x,Q+20)==r(x,Q+28)==0x5a5a5a5a
setup(commands[0]);failure=True;before=bytearray(x.mem_read(Q,64));before[32:36]=w(wire[0]);before[12:16]=w(wire[1]);assert run()==0xffffffff and bytes(x.mem_read(Q,64))==before and calls==1
report=dict(result='PASS',original_pc_nxdk_cases=2048,special_modes=specials,callback_error_guards=1,scope='Full40aae0, actual42a060/42a0a0 and vector copy. Weapon-presence callback supplied and mutates movement, dimensions, position/tokens. Exact original actor/query footprint, shared PC/NXDK outputs and retained query fields. No live scene binding.')
(root/'artifacts/ai-prepare.json').write_text(json.dumps(report,indent=2));print(report)

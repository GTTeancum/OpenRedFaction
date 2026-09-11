"""Original4972a0 room query and field commit vs shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0x30000000,65536);return u
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');b=0x30000000;stack=b+0xe000;stop=b+0xf000;returned=0;failure=0;trace=b''
entry=int(re.search(r'_rf_particle_emitter_move\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def hook(m,address,size,context):
 global trace
 if address not in (0x4cd970,b+0x3000):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<7I',m.mem_read(sp,28))
 if address==0x4cd970:
  trace=w(1,a[1])+bytes(m.mem_read(a[2],12))+bytes(m.mem_read(a[3],12))+w(a[4]);result=returned;pop=20
 else:
  trace=w(1,a[2])+bytes(m.mem_read(a[3],12))+bytes(m.mem_read(a[4],12))+w(a[5]);m.mem_write(a[6],w(returned));result=failure;pop=4
 m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+pop);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4972a0);cases=[];expected=[]
for i in range(1024):
 vectors=[struct.pack('<3f',*[rng.uniform(-100,100) for _ in range(3)]) for j in range(4)]
 if i%3==0:vectors[3]=vectors[1]
 if i%5==0:vectors[2]=vectors[0]
 old_room=rng.choice([0,1,0xffffffff,0x12345678]);returned=rng.choice([0,1,0xffffffff,0x87654321])
 wire=vectors[0]+vectors[1]+w(old_room)+vectors[2]+vectors[3]+w(returned,0);cases.append(wire)
 u.mem_write(b,bytes(0x180));u.mem_write(b+8,wire[:24]);u.mem_write(b+76,w(old_room));u.mem_write(b+0x1000,wire[28:52]);u.mem_write(stack,w(stop,b+0x1000,b+20 if i%3==0 else b+0x100c));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);trace=bytes(36);u.emu_start(0x4972a0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 want=w(0)+bytes(u.mem_read(b+8,24))+bytes(u.mem_read(b+76,4))+trace
 assert want==w(0)+wire[28:52]+w(returned,1,old_room)+wire[:12]+wire[28:40]+w(0)
 expected.append(want)
for offset in (0,28,40):
 wire=bytearray(cases[0]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:28]+bytes(36))
wire=bytearray(cases[0]);wire[56:60]=w(-1);cases.append(bytes(wire));expected.append(w(-1)+wire[:28]+expected[0][32:])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--emitter-move'],input=b''.join(cases));assert actual==b''.join(expected),'PC mismatch'
for i,(wire,want) in enumerate(zip(cases,expected)):
 returned,failure=struct.unpack('<2I',wire[52:]);x.mem_write(b,bytes(512));x.mem_write(b+4,wire[:24]);x.mem_write(b+72,wire[24:28]);x.mem_write(b+0x1000,wire[28:52]);trace=bytes(36)
 x.mem_write(stack,w(stop,b,b+0x1000,b+16 if i<1024 and i%3==0 else b+0x100c,b+0x3000,0));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+4,24))+bytes(x.mem_read(b+72,4))+trace;assert got==want,('NXDK',i)
report=dict(result='PASS',original_cases=1024,guard_cases=4,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete4972a0 with real copy helpers; supplied4cd970 locator. Exact old-room/from/to/flag0 query, new position/direction/room and same-direction alias; unchanged-position still queries, room0 accepted. PC/NXDK match; nonfinite and locator-failure guards. No actual world room traversal, parent-room update or emission.')
(root/'artifacts/emitter-move.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

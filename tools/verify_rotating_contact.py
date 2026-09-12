"""Original49d7e0 rotating zero-inverse-mass branch; real helpers, no hooks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_physics_rotating_contact\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x49dbaa);commands=[];expected=[];changed=damped=0
for case in range(4096):
 state=bytearray(rng.getrandbits(8) for _ in range(308))
 values=[rng.randrange(-1024,1025)/32 for _ in range(18)]
 matrix=[rng.randrange(-32,33)/16 for _ in range(9)]
 if case%4==0:matrix=[1,0,0,0,1,0,0,0,1]
 count=(-1,0,1,2,3,32)[case%6];mode=(1,3,8)[case%3]
 state[88:100]=floats(values[:3]);state[112:148]=floats(matrix);state[184:208]=floats(values[3:9]);state[272:276]=pack(struct.unpack_from('<I',state,272)[0]&~0x80)
 point=values[9:12];normal=[rng.randrange(-32,33)/16 for _ in range(3)];support=values[12:15];contact=values[15:18]
 if case%17==0:normal=[0,0,0]
 command=bytes(state)+floats(point+normal+support+contact)+struct.pack('<i',count)
 actor=bytearray(rng.getrandbits(8) for _ in range(0x1000))
 for off,data in ((0x24,pack(0)),(0x294,pack(base+0x3000)),(0x858,pack(base+0x4000)),(0xe4,state[88:100]),(0xfc,state[112:148]),(0x144,state[184:208]),(0x1a8,state[272:276]),(0x1b4,floats(point)),(0x1c0,floats(normal)),(0x1d4,pack(0)),(0x1d8,floats(contact)),(0x8a0,floats(support)),(0x1e4,pack(0xffffffff)),(0x1ec,pack(0)),(0x184,struct.pack('<i',count))):actor[off:off+len(data)]=data
 u.mem_write(base,bytes(actor));u.mem_write(base+0x31b4,pack(1));u.mem_write(base+0x4004,pack(mode))
 u.mem_write(stack,pack(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x49d7e0,0x49ddef,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==0x49ddef
 out=bytes(u.mem_read(base,len(actor)));impact=bytes(u.mem_read(u.reg_read(UC_X86_REG_ESP)+0x10,4))
 want=bytearray(state);want[184:208]=out[0x144:0x15c];actor[0x144:0x15c]=out[0x144:0x15c];assert out==actor,('unrelated original mutation',case)
 changed+=want[184:196]!=state[184:196];damped+=want[196:208]!=state[196:208]
 result=bytes(want)+impact
 x.mem_write(base,b'\xa5'*16+command+b'\x5a'*16);x.mem_write(base+0x2000,pack(0xa5a5a5a5))
 x.mem_write(stack,pack(stop,base+16,base+16+308,base+16+320,base+16+332,base+16+344,count&0xffffffff,base+0x2000))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,('return',case)
 got=bytes(x.mem_read(base+16,308))+bytes(x.mem_read(base+0x2000,4));assert got==result,('NXDK',case,got[184:208].hex(),result[184:208].hex(),got[308:].hex(),result[308:].hex())
 assert bytes(x.mem_read(base,16))==b'\xa5'*16 and bytes(x.mem_read(base+376,16))==b'\x5a'*16
 assert bytes(x.mem_read(base+324,52))==command[308:]
 commands.append(command);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--rotating-contact'],input=b''.join(commands));assert pc==b''.join(expected),'PC mismatch'
guards=0
for offset in list(range(88,100,4))+list(range(112,148,4))+list(range(184,208,4))+list(range(308,356,4)):
 bad=bytearray(commands[0]);bad[offset:offset+4]=pack(0x7fc00000)
 x.mem_write(base,bytes(bad));x.mem_write(base+0x2000,pack(0xa5a5a5a5))
 x.mem_write(stack,pack(stop,base,base+308,base+320,base+332,base+344,2,base+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xfffffffc
 assert bytes(x.mem_read(base,360))==bad and bytes(x.mem_read(base+0x2000,4))==pack(0xa5a5a5a5)
 guards+=1
report=dict(result='PASS',cases=len(commands),nonfinite_guards=guards,velocity_changes=changed,angular_damped=damped,original_sha256=sha,scope='Original49d7e0 entry through49ddef, zero inverse mass, flag80 clear, real429990/486c90 kind1 predicates, cross product, orientation and numerical helpers, no hooks. Exact PC/NXDK complete body and signed impact, unchanged original unrelated bytes and NXDK sources/guards. Counts -1/0/1/2/3/32, identity/arbitrary finite orientations, zero/nonunit normals. Crush/stance and damage effects excluded.')
(root/'artifacts/rotating-contact-verification.json').write_text(json.dumps(report,indent=2));print(report)

"""Full40b3d0 pair geometry versus shared PC/compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;node=base+0x100;out=base+0x200;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_pair\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(address,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x40b3d0);commands=[];expected=[];counts=[0,0,0];coincident=0
u.mem_write(0x1754474,b'\x07');second=base+0x180
for case in range(8192):
 a=bytearray(rng.randbytes(68));b=bytearray(rng.randbytes(68));p=[rng.uniform(-8,8) for _ in range(3)];pa=[rng.uniform(-4,4) for _ in range(3)];pb=[rng.uniform(-4,4) for _ in range(3)];radius=rng.uniform(-1,4)
 if case%8==0:pb=pa[:];coincident+=1
 if case%8==1:pa=[0,0,0];pb=[0,rng.choice((-4,4)),0]
 if case%4==2:p=[(pa[i]+pb[i])*.5 for i in range(3)]
 a[:12]=f(*pa);b[:12]=f(*pb);a[28:36]=f(rng.uniform(-1,4),rng.uniform(-1,8));b[28:36]=f(rng.uniform(-1,4),rng.uniform(-1,8))
 wire=f(*p,radius)+a+b;radius_word=struct.unpack_from('<I',wire,12)[0]
 u.mem_write(base,wire[:12]);u.mem_write(node,bytes(a));u.mem_write(second,bytes(b));u.mem_write(out,w(0xa5a5a5a5))
 result=call(u,0x40b3d0,[base,radius_word,node,second,out]);assert result in (0,1,2);score=bytes(u.mem_read(out,4));counts[result]+=1
 assert bytes(u.mem_read(node,68))==a and bytes(u.mem_read(second,68))==b
 if result!=1:assert score==w(0xa5a5a5a5)
 x.mem_write(base,wire[:12]);x.mem_write(node,bytes(a));x.mem_write(second,bytes(b));x.mem_write(out,w(0xa5a5a5a5,99))
 assert call(x,entry,[base,radius_word,node,second,out,out+4])==0,(case,'port error')
 assert bytes(x.mem_read(out,8))==score+w(result),(case,result,score.hex(),bytes(x.mem_read(out,8)).hex())
 assert bytes(x.mem_read(node,68))==a and bytes(x.mem_read(second,68))==b
 commands.append(wire);expected.append(w(0,result)+score)
for offset in (0,4,8,12,16,20,24,44,48,84,88,92,112,116):
 wire=bytearray(commands[0]);wire[offset:offset+4]=w(0x7fc12345);x.mem_write(base,bytes(wire[:12]));x.mem_write(node,bytes(wire[16:84]));x.mem_write(second,bytes(wire[84:]));x.mem_write(out,w(0xa5a5a5a5,99))
 status=call(x,entry,[base,struct.unpack_from('<I',wire,12)[0],node,second,out,out+4]);assert status!=0 and bytes(x.mem_read(out,8))==w(0xa5a5a5a5,99)
 commands.append(bytes(wire));expected.append(w(status,99,0xa5a5a5a5))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-pair'],input=b''.join(commands));assert actual==b''.join(expected),'PC mismatch'
report=dict(result='PASS',cases=8192,classifications=counts,coincident_pairs=coincident,nonfinite_guards=14,scope='Full original40b3d0 and all geometry/vector callees, no hooks;507a50 static constructors preinitialized.53-bit x87. Exact result/conditional score versus PC/compiled NXDK; full candidate preservation, negative dimensions, vertical directions, midpoint queries and coincident centers. No navigation list selection, visibility fallback, scene ownership or native XEMU run.')
(root/'artifacts/navigation-pair-verification.json').write_text(json.dumps(report,indent=2));print(report)

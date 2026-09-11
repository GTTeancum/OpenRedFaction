"""Original static/rising/falling mover support position and flag commits."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
source=root/'Installed_Game/RF.exe';assert hashlib.sha256(source.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(source);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_physics_support_commit\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x4a0ae3);commands=bytearray();expected=bytearray();raised=0
for n in range(2048):
 state=bytearray(308);position=f(*(rng.uniform(-100,100) for _ in range(3)))
 state[100:112]=position;state[184:196]=f(1,-2,3);state[244:248]=f(.5);state[272:276]=w(rng.getrandbits(32))
 y=struct.unpack('<3f',position)[1];probe=bytearray(84);probe[:24]=f(0,y+rng.uniform(0,2),0,0,y-rng.uniform(0,2),0)
 fraction=f(rng.random());moving=n%2;contact_y=f([-2,0,2,1e-8][(n//2)%4]);handle=n+100
 command=bytes(state)+bytes(probe)+fraction+w(moving)+contact_y+w(handle);commands.extend(command)
 u.mem_write(b,bytes(0x4000));u.mem_write(b+0xf0,position);u.mem_write(b+0x144,bytes(state[184:196]));u.mem_write(b+0x180,bytes(state[244:248]));u.mem_write(b+0x1a8,bytes(state[272:276]));u.mem_write(b+0x302c,w(handle))
 u.mem_write(stack,bytes(256));u.mem_write(stack+0xc,bytes(probe[:24]));u.mem_write(stack+0x60,fraction);u.mem_write(stack+0x70,contact_y);u.mem_write(stack+0x64,w(n%10));u.mem_write(b+0x1380,w(0xa5a5a5a5))
 u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EDI,b+0x3000);u.reg_write(UC_X86_REG_EBX,b+0xf0);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4a0ae3 if moving else 0x4a0b31,0x4a0c05,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4a0c05
 assert bytes(u.mem_read(b+0x1380,4))==w(n%10), ("ground material",n)
 value=bytearray(state)
 for dst,src,size in [(88,0xe4,12),(100,0xf0,12),(248,0x190,24),(272,0x1a8,4)]:value[dst:dst+size]=bytes(u.mem_read(b+src,size))
 raised+=struct.unpack_from('<f',value,104)[0]>y
 want=w(0)+value+bytes(u.mem_read(b+0x8ac,4));expected.extend(want)
 x.mem_write(b,bytes(state));x.mem_write(b+0x2000,bytes(probe));x.mem_write(b+0x3000,w(0xa5a5a5a5))
 x.mem_write(stack,w(stop,b,b+0x2000)+fraction+w(moving)+contact_y+w(handle,b+0x3000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop,(n,hex(x.reg_read(UC_X86_REG_EIP)))
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,308))+bytes(x.mem_read(b+0x3000,4));assert got==want,('NXDK',n)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--support-commit'],input=commands)
assert actual==expected,'PC support differs'
report=dict(result='PASS',cases=2048,raised=raised,material_transfers=2048,scope='Original4a0ae3/4a0b31 through4a0c05 verifies contact material transfer to entity+1380 before landing predicate. Existing PC/NXDK numeric support comparison remains limited to4a0bfa; unchanged vector/bounds/min callees. PC/NXDK exact state and support handle for static and resolved mover contacts including positive/nonpositive Y. Lookup, entity rejection, contact-record copy and landing effects excluded.')
(root/'artifacts/support-commit-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

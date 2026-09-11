"""Original actor force carry branch vs PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_physics_force_actor_carry\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x486b73);commands=bytearray();expected=bytearray()
for case in range(2048):
    mode=case%10;kind=(case//10)%3;attachment=0xffffffff if case%2 else 100
    support=f(*(rng.uniform(-10,10) for _ in range(3)));flags=rng.getrandbits(32)
    influence=f(*(rng.uniform(-1,1) for _ in range(3)),rng.uniform(-20,20))
    actor=bytearray(rng.randbytes(0x1500));actor[0x24:0x28]=w(0)
    actor[0x294:0x298]=w(base+0x2000);actor[0x858:0x85c]=w(base+0x3000)
    actor[0x8a0:0x8ac]=support;actor[0x1380:0x1384]=w(attachment);actor[0x1a8:0x1ac]=w(flags)
    u.mem_write(base,bytes(actor));u.mem_write(base+0x21b4,w(kind));u.mem_write(base+0x3004,w(mode))
    u.mem_write(stack+0x1c,influence[:12]);u.mem_write(stack+0x50,influence[12:])
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,base)
    u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x486b73,0x486c1c,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x486c1c
    result=bytes(u.mem_read(base+0x8a0,12))+bytes(u.mem_read(base+0x1a8,4))
    actor[0x8a0:0x8ac]=result[:12];actor[0x1a8:0x1ac]=result[12:]
    assert bytes(u.mem_read(base,len(actor)))==actor,('unexpected actor change',case)
    args=support+w(flags)+influence+w(mode,kind,attachment);commands.extend(args);expected.extend(result)
    x.mem_write(base,args);x.mem_write(stack,w(stop,base,base+12,base+16,mode,kind,attachment))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    actual=bytes(x.mem_read(base,16))
    assert actual==result,(case,struct.unpack('<3fI',actual),struct.unpack('<3fI',result))
    assert bytes(x.mem_read(base+16,len(args)-16))==args[16:]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-carry'],input=commands)
assert actual==expected,'PC differs'
report=dict(result='PASS',cases=2048,original_sha256=digest,
 scope='Original486b73..486c1c including unchanged mode/class predicates, vector/norm/scaling callees. All actor bytes preserved except support vector+8a0 and dirty bit+1a8. Modes0..9, class kinds0..2, attached/unattached, signed strength. Exact PC/NXDK results. Type0 actor/class mapping supplied; no selection, eligibility, rotation, 0x40 branch or campaign integration.')
(root/'artifacts/force-carry.json').write_text(json.dumps(report,indent=2));print(report)

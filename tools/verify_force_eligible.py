"""Original force eligibility prefix with actual query/registry/player list."""
import hashlib,json,re,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
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
entry=int(re.search(r'_rf_physics_force_eligible\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def reached(cpu,address,size,data):cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,reached,begin=0x486940,end=0x486940)
u.hook_add(UC_HOOK_CODE,reached,begin=0x486c1c,end=0x486c1c)
commands=bytearray();expected=bytearray();accepted=0;case=0
import itertools
for flags,present,regionflags,related,kind,mode in itertools.product(
    (0,8,0x8000000,0x8000008,0x10000008,0xffffffff),(0,1),(0,2,0x20,0x22),range(3),range(3),range(10)):
    actor=bytearray(0x1500);actor[0x1a8:0x1ac]=w(flags);actor[0x2c:0x30]=w(0x70000)
    actor[0x24:0x28]=w(0 if kind==0 else 9);actor[0x7c:0x80]=w(8 if related==1 else 0)
    actor[0x858:0x85c]=w(base+0x6000);u.mem_write(base,bytes(actor));u.mem_write(base+0x6004,w(mode))
    u.mem_write(0x7394cc,w(base if kind!=2 else 0,base+0x4000))
    owner=bytearray(0x300);owner[0x2c:0x30]=w(0x70001);owner[0x200:0x204]=w(0x70000 if related==2 else 0xffffffff)
    u.mem_write(base+0x4000,bytes(owner));node=bytearray(0x20);node[:4]=w(base+0x3000);node[0x14:0x18]=w(0x70001)
    u.mem_write(base+0x3000,bytes(node));u.mem_write(0x7c75cc,w(base+0x3000))
    region=w(1,10,regionflags)+f(*([0]*12),1,*([0]*10))+w(1)
    assert len(region)==108
    u.mem_write(base+0x2000,region);u.mem_write(base+0x5000,w(base+0x2000));u.mem_write(0x6460bc,w(present,1,base+0x5000))
    u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x4868c0,stop,count=10000)
    pc=u.reg_read(UC_X86_REG_EIP);assert pc in (0x486940,0x486c1c)
    result=int(pc==0x486940);accepted+=result
    assert bytes(u.mem_read(base,len(actor)))==actor
    args=w(flags,present,regionflags,int(related!=0),int(kind==0),mode);commands.extend(args);expected.extend(w(result))
    x.mem_write(stack,w(stop)+args);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==result,case
    case+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-eligible'],input=commands)
assert actual==expected,'PC differs'
report=dict(result='PASS',cases=case,accepted=accepted,original_sha256=digest,
 scope='Original4868c0 prefix until eligibility success/reject, all query/sphere/registry/type/mode/player-list callees unchanged. Body flag gates, empty region list, region flags2/20, object flag8 and indirect player attachment, valid actor/nonactor/empty registry, modes0..9. No actor mutation. PC/NXDK boolean agrees using resolved predicates; this does not port list traversal or register regions in campaign.')
(root/'artifacts/force-eligible.json').write_text(json.dumps(report,indent=2));print(report)

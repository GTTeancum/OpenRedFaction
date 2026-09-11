"""Original dead/dying queries with real handle lookup versus PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;actor=b+0x2000;registry=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096)
    m.mem_write(base,im);m.mem_map(b,65536);return m
def run(m,address,args):
    m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack)
    m.emu_start(address,stop,count=10000);assert m.reg_read(UC_X86_REG_EIP)==stop
    return m.reg_read(UC_X86_REG_EAX)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary)
mapping=(root/'build/xbox/main.map').read_text()
entries=[int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('rf_player_is_dead','rf_player_is_dying')]
rng=random.Random(0x4a4920);commands=[];expected=[];counts=[0,0]
for case in range(4096):
    present=rng.randrange(2);handle=rng.choice((0,1023,1024,0x10007,0x80000007,0xffffffff))
    stored=handle^rng.choice((0,0,0x10000));kind=rng.choice((0,0,1,4));flags=rng.getrandbits(32);occupied=rng.randrange(2)
    row=w(present,handle,stored,kind,flags,occupied);commands.append(row);slot=handle&0xffff
    owner=bytearray(rng.randbytes(0x1204));owner[0x14:0x18]=w(handle)
    body=bytearray(rng.randbytes(0x1500));body[0x24:0x28]=w(kind);body[0x2c:0x30]=w(stored);body[0x810:0x814]=w(flags)
    body[0x34:0x38]=w(rng.choice((0,0x42c80000,0xbf800000,0x7fc00000)))
    u.mem_write(b,bytes(owner));u.mem_write(actor,bytes(body));u.mem_write(0x7394cc,bytes(4096))
    x.mem_write(registry,bytes(4096));x.mem_write(b,w(handle));x.mem_write(actor,w(stored,kind,0,0,flags)+bytes(36))
    if occupied and slot<1024:
        u.mem_write(0x7394cc+slot*4,w(actor));x.mem_write(registry+slot*4,w(actor))
    before=bytes(x.mem_read(b,0x6000))
    want=[run(u,a,[b if present else 0])&255 for a in (0x4a4920,0x4a4940)]
    got=[run(x,a,[registry,b if present else 0]) for a in entries]
    assert got==want,(case,row.hex(),want,got)
    assert bytes(u.mem_read(b,len(owner)))==owner and bytes(u.mem_read(actor,len(body)))==body
    assert bytes(x.mem_read(b,0x6000))==before
    expected.append(w(*want));counts=[a+c for a,c in zip(counts,want)]
assert all(counts)
probe=root/'build/pc/Release/rf_entity_probe.exe'
assert subprocess.check_output([str(probe),'--player-phase'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',cases=4096,dead=counts[0],dying=counts[1],original_sha256=sha,
    nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
    scope='Full original4a4920/4a4940 with unchanged426fc0/40a0e0; no replaced callees. Real-layout player/entity and handle table fixtures including stale generations, invalid slots, type rejection, null player and varied health/flags. Exact normalized PC/NXDK queries; owners unchanged. No death transition, removal, control or camera lifecycle.')
(root/'artifacts/player-phase.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

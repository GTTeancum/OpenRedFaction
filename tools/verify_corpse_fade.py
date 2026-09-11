"""Original corpse health/fade prefix with real flag helpers versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;out=b+0x2000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,im);m.mem_map(b,65536)
    m.reg_write(UC_X86_REG_FPCW,0x27f);return m
def halt(m,a,size,data):
    if a==stop:m.emu_stop()
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary);u.hook_add(UC_HOOK_CODE,halt)
entry=int(re.search(r'\s_rf_corpse_fade_step\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def shared(wire):
    x.mem_write(b,wire[:16]);x.mem_write(out,w(0xaaaaaaaa));x.mem_write(stack,w(stop,b,struct.unpack('<I',wire[16:])[0],out));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,16))+bytes(x.mem_read(out,4))
rng=random.Random(0x417290);inputs=[];outputs=[];early=marked=continued_marked=0
edges=[0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x3c888889,0x00800000,0x007fffff]
for case in range(8192):
    health=rng.choice((0xbf800000,0x80000000,0,0x42c80000));flags=rng.getrandbits(32);obj_flags=rng.getrandbits(32)&~2
    fade=rng.choice(edges) if case%2 else rng.randrange(0x7e800000)|(rng.randrange(2)<<31)
    dt=(fade&0x7fffffff) if case%4==0 else rng.choice((0,1,0x3c888889,0x3f800000,rng.randrange(0x7e800000)))
    if (health==0xbf800000 or not flags&1) and case%5==0:dt=0x7fc00000;fade=0x7fc00000
    wire=w(health,fade,obj_flags,flags,dt);body=bytearray(rng.randbytes(0x400))
    for off,value in ((0x34,health),(0x298,fade),(0x7c,obj_flags),(0x29c,flags)):body[off:off+4]=w(value)
    u.mem_write(b,bytes(body));u.mem_write(0x5a4014,w(dt));u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x417290,0x4172ea,count=10000);ip=u.reg_read(UC_X86_REG_EIP);assert ip in (stop,0x4172ea)
    proceed=int(ip==0x4172ea);got=bytes(u.mem_read(b,len(body)))
    expected_body=bytearray(body);expected_body[0x298:0x29c]=got[0x298:0x29c];expected_body[0x7c:0x80]=got[0x7c:0x80];assert got==expected_body
    state=got[0x34:0x38]+got[0x298:0x29c]+got[0x7c:0x80]+got[0x29c:0x2a0];want=w(0)+state+w(proceed)
    assert shared(wire)==want,(case,wire.hex(),shared(wire).hex(),want.hex())
    early+=not proceed;deleted=bool(struct.unpack('<I',got[0x7c:0x80])[0]&2);marked+=deleted;continued_marked+=deleted and proceed
    inputs.append(wire);outputs.append(want)
for wire in (w(0x7fc00000,0,0,0,0),w(0,0x7f800000,0,1,0),w(0,0x3f800000,0,1,0xbf800000)):
    want=w(-4)+wire[:16]+w(0xaaaaaaaa);assert shared(wire)==want;inputs.append(wire);outputs.append(want)
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-fade'],input=b''.join(inputs))==b''.join(outputs)
report=dict(result='PASS',original_cases=8192,guards=3,early_returns=early,deletion_marks=marked,continued_after_mark=continued_marked,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Original417290..4172ea or early return, real4174e0/48ab40, no replaced callees. Exact PC/NXDK fade/flags and continuation gate across finite health, signed zero, subnormals, zero crossings and unused NaNs; active dt nonnegative with representable remainder. Three PC/NXDK guards. Remaining corpse update/render/deletion excluded.')
(root/'artifacts/corpse-fade.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

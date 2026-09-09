"""Complete original segment/AABB function, including failed-attempt writes."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000
rng=random.Random(0x508b70);cases=[];expected=[];hits=0;changed_misses=0;guards=0
for n in range(16000):
    scale=2.0**rng.randrange(-15,16)
    lo=[rng.uniform(-10,0)*scale for _ in range(3)];hi=[rng.uniform(0,10)*scale for _ in range(3)]
    if n%7==0:hi[n%3]=lo[n%3]
    start=[rng.uniform(-20,20)*scale for _ in range(3)];end=[rng.uniform(-20,20)*scale for _ in range(3)]
    if n%5==0:start=[rng.choice([lo[j],hi[j],(lo[j]+hi[j])/2]) for j in range(3)]
    if n%11==0:end=start[:]
    if n%13==0:end=[(lo[j]+hi[j])/2 for j in range(3)]
    if n%17==0:start[n%3]=end[n%3]
    if n%97==0:lo[0]=math.nan
    if n%101==0:lo[1]=hi[1]+scale
    wire=struct.pack('<15f',*lo,*hi,*start,*end,123,456,789);cases.append(wire)
    if n%97==0 or n%101==0:
        expected.append(struct.pack('<iI',-2,0xa5a5a5a5)+wire[48:]);guards+=1;continue
    u.mem_write(base,wire);u.mem_write(stack,struct.pack('<6I',stop,base,base+12,base+24,base+36,base+48))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x508b70,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1)
    point=bytes(u.mem_read(base+48,12));hits+=hit;changed_misses+=not hit and point!=wire[48:]
    expected.append(struct.pack('<iI',0,hit)+point)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe')],input=b''.join(cases))
assert len(actual)==len(expected)*20
for n,want in enumerate(expected):assert actual[n*20:(n+1)*20]==want,(n,cases[n].hex(),actual[n*20:(n+1)*20].hex(),want.hex())
# Execute the actual NXDK-linked x86 routine; no replacement callees.
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_segment_box\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
    x.mem_write(base,wire);x.mem_write(base+64,struct.pack('<I',0xa5a5a5a5))
    x.mem_write(stack,struct.pack('<7I',stop,base,base+12,base+24,base+36,base+48,base+64))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
    x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+64,4))+bytes(x.mem_read(base+48,12))
    assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',nxdk_cases=len(cases),nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),original_cases=len(cases)-guards,hits=hits,misses=len(cases)-guards-hits,misses_writing_point=changed_misses,port_guards=guards,scope='Complete unchanged 508b70 and outcode/vector callees; exact hit byte and all output float bytes, including misses. Finite fixtures span scales 2^-15..2^15, boundaries, zero-length segments and degenerate boxes; not a world collision query.')
(root/'artifacts/collision-box-verification.json').write_text(json.dumps(report,indent=2));print(report)

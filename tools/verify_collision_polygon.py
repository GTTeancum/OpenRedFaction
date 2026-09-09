"""Original one-sided segment-plane test and actual NXDK implementation."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000


rng=random.Random(0x4e1f50);cases=[];expected=[];hits=0;guards=0
for n in range(7000):
    count=rng.randrange(1,17);normal=rng.choice([[1,0,0],[0,1,0],[0,0,1],[-1,0,0],[0,-1,0],[0,0,-1],[1,1,1],[1,-1,0],[0,0,0]])
    vertices=[[rng.uniform(-10,10) for _ in range(3)] for _ in range(count)]
    # Ordered concave star polygons, with reversed winding and exact edge probes.
    if n%2==0:
        axis=n%3
        for i in range(count):
            r=5 if i%2 else 9;v=[r*math.cos(2*math.pi*i/count),r*math.sin(2*math.pi*i/count),0]
            vertices[i]=v[axis:]+v[:axis]
    if n%3==0:vertices.reverse()
    point=[rng.uniform(-12,12) for _ in range(3)]
    if n%5==0:point=vertices[0][:]
    if n%7==0:point=[(vertices[0][i]+vertices[-1][i])/2 for i in range(3)]
    if n%97==0:count=0
    if n%101==0:normal=[math.nan,0,1]
    flat=[v for vertex in vertices for v in vertex]+[0.0]*(48-len(vertices)*3)
    wire=struct.pack('<54fI',*normal,*point,*flat,count);cases.append(wire)
    if count==0 or n%101==0:
        expected.append(struct.pack('<iI',-4 if count==0 else -2,0xa5a5a5a5));guards+=1;continue
    u.mem_write(base,wire[:216]);face=base+4096;edges=base+8192
    u.mem_write(face,wire[:12]);u.mem_write(face+0x40,struct.pack('<I',edges))
    for i in range(count):
        u.mem_write(edges+i*32,struct.pack('<I',base+24+i*12))
        u.mem_write(edges+i*32+0x14,struct.pack('<II',edges+((i+1)%count)*32,edges+((i-1)%count)*32))
    u.mem_write(stack,struct.pack('<II',stop,base+12));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,face);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x4e1f50,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);hits+=hit
    expected.append(struct.pack('<iI',0,hit))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--polygon'],input=b''.join(cases))
assert len(actual)==len(expected)*8
for n,want in enumerate(expected):assert actual[n*8:(n+1)*8]==want,('PC',n,cases[n].hex(),actual[n*8:(n+1)*8].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_polygon_contains\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
    x.mem_write(base,wire);x.mem_write(base+256,struct.pack('<I',0xa5a5a5a5))
    count=struct.unpack_from('<I',wire,216)[0]
    x.mem_write(stack,struct.pack('<6I',stop,base,base+12,base+24,count,base+256))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+256,4))
    assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=len(cases)-guards,inside=hits,port_guards=guards,nxdk_cases=len(cases),nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Complete 4e1f50 with unchanged axis selection and original circular edge lists; projected containment, including concave/reversed/degenerate and exact boundary fixtures. Not world traversal.')
(root/'artifacts/collision-polygon-verification.json').write_text(json.dumps(report,indent=2));print(report)

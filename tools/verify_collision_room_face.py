"""Execute original 4e3800 with null reference face, matching containing-room use."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(raw)+4095)//4096*4096);u.mem_write(0x400000,raw)
base=0x30000000;face=base+0x1000;verts=base+0x2000;edges=base+0x3000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
f32=lambda v:struct.unpack('<f',f(v))[0]
rng=random.Random(0x4e3800);inputs=[];expected=[];retries=0;selected=0
for case in range(1200):
    axis=case%3;others=[i for i in range(3) if i!=axis];sign=1 if case%2 else -1
    height=f32(rng.uniform(-20,20));plane=[0.,0.,0.,-sign*height];plane[axis]=sign
    vertices=[]
    for a,b in [(-2,-2),(2,-2),(2,2),(-2,2)]:
        v=[0.,0.,0.];v[axis]=height;v[others[0]]=a;v[others[1]]=b;vertices.append(v)
    lo=[min(v[i] for v in vertices) for i in range(3)];hi=[max(v[i] for v in vertices) for i in range(3)]
    start=[f32(rng.uniform(-3,3)) for _ in range(3)];start[axis]=f32(height+rng.choice([-2.,-0.00005,0.,0.00005,2.]))
    direction=[0.,0.,0.];direction[axis]=rng.choice([-1.,1.]);end=[f32(start[i]+direction[i]*10) for i in range(3)]
    if case%5==0:start[others[0]]=end[others[0]]=rng.choice([-2.,-1.99999,0.,1.99999,2.]);start[others[1]]=end[others[1]]=0.
    if case%2:
        angle=rng.uniform(-3,3);c=math.cos(angle);sn=math.sin(angle)
        def rotate(v):return [f32(c*v[0]-sn*v[1]),f32(sn*v[0]+c*v[1]),v[2]]
        start=rotate(start);direction=rotate(direction);end=rotate(end)
        vertices=[rotate(v) for v in vertices];plane=rotate(plane[:3])+plane[3:]
        lo=[min(v[i] for v in vertices) for i in range(3)];hi=[max(v[i] for v in vertices) for i in range(3)]
    old=88 if case%3==0 else 0;distance=f32(rng.choice([0.,2.,2.00005,1.99995,10.]));front=case%2;hits=7
    query=f(*start,*direction,*end)+w(old)+f(distance)+w(front,hits)
    wire=query+f(*plane,*lo,*hi,*[a for v in vertices for a in v])+w(77);inputs.append(wire)
    u.mem_write(base,bytes(0x4000));u.mem_write(base,query[:36]);u.mem_write(base+0x28,w(face+0x100 if old else 0));u.mem_write(base+0x2c,f(distance));u.mem_write(base+0x30,bytes([front,0]));u.mem_write(base+0x34,w(hits))
    u.mem_write(face,f(*plane,*lo,*hi));u.mem_write(face+0x40,w(edges));u.mem_write(verts,f(*[a for v in vertices for a in v]))
    for i in range(4):u.mem_write(edges+i*32,w(verts+i*12,0,0,0,0,edges+(i+1)%4*32,edges+(i-1)%4*32))
    u.mem_write(stack,w(stop,face));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
    
    try:u.emu_start(0x4e3800,stop,count=100000)
    except Exception:
        print('case/eip',case,hex(u.reg_read(UC_X86_REG_EIP)));raise
    assert u.reg_read(UC_X86_REG_EIP)==stop
    retry=u.reg_read(UC_X86_REG_EAX)&255;retries+=retry
    chosen=struct.unpack('<I',u.mem_read(base+0x28,4))[0];token=77 if chosen==face else 88 if chosen else 0;selected+=token==77
    out=w(0,retry)+bytes(u.mem_read(base,36))+w(token)+bytes(u.mem_read(base+0x2c,4))+w(u.mem_read(base+0x30,1)[0])+bytes(u.mem_read(base+0x34,4));expected.append(out)
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_collision_probe.exe'),'--room-face'],input=b''.join(inputs))
assert len(actual)==60*len(inputs)
for i,e in enumerate(expected):assert actual[i*60:(i+1)*60]==e,('PC mismatch',i,actual[i*60:(i+1)*60].hex(),e.hex())
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));raw=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_collision_room_query_face\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
for i,wire in enumerate(inputs):
    x.mem_write(base,wire[:52]);x.mem_write(face,wire[52:92]+w(verts,4)+bytes(24));x.mem_write(verts,wire[92:140]);x.mem_write(base+0x5000,w(99))
    x.mem_write(stack,w(stop,base,face,77,base+0x5000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x5000,4))+bytes(x.mem_read(base,52))
    assert got==expected[i],('NXDK mismatch',i,got.hex(),expected[i].hex())
report=dict(result='PASS',pc_cases=len(inputs),nxdk_cases=len(inputs),selected=selected,retries=retries,scope='Unmodified original 4e3800 and geometric callees, null reference face. Axis-aligned and rotated quads, plane/boundary/tie fixtures. Byte-exact state comparisons for these cases; full arithmetic domain and complete room traversal remain unverified.')
(ROOT/'artifacts/collision-room-face-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

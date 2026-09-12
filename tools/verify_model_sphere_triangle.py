"""Full original54de40 and all geometry helpers versus shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;batch=b+0x1000;vertices=b+0x2000;planes=b+0x3000;records=b+0x4000;query=b+0x5000;hit=b+0x6000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_model_sphere_triangle\s+([0-9a-fA-F]+)',mapping)[1],16)
u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
rng=random.Random(0x54de40);commands=[];answers=[];hits=0;flipped=0;edge_normals=0
for case in range(8192):
    axis=case%3;a=(axis+1)%3;c=(axis+2)%3
    plane=[0.,0.,0.,0.];plane[axis]=(-2.,-1.,.5,1.)[case//3%4]
    triangle=[[0.,0.,0.] for _ in range(3)]
    for v,uv in zip(triangle,((-2.,-2.),(2.,-2.),(0.,2.))):v[a],v[c]=uv
    start=[rng.randrange(-24,25)/8 for _ in range(3)];delta=[0.,0.,0.]
    start[axis]=(-2.,-1.,0.,1.,2.)[case//12%5];delta[axis]=(-4.,-2.,0.,2.,4.)[case//60%5]
    if case>=4096:
        triangle=[[rng.uniform(-5,5) for _ in range(3)] for _ in range(3)]
        plane=[rng.uniform(-1,1) for _ in range(3)]+[rng.uniform(-2,2)]
        delta=[rng.uniform(-5,5) for _ in range(3)]
    two=(0,1,0x100,0x20)[case//300%4];index=case%4;token=records+index*8
    limit=(0.,.25,.5,.75,1.,2.)[case//7%6]
    initial=f([limit,11,12,13,14,15,16])+w(0x12345678)
    radius=(0,.001,.25,.5,1,2)[case%6]
    command=f(start+delta+plane+[v for row in triangle for v in row])+w(token,two)+initial+f([radius]);assert len(command)==120;commands.append(command)
    ids=(-1,0,1) if case%2 else (2,0,1)
    u.mem_write(batch,b'\xa5'*0x38);u.mem_write(batch+4,w(vertices));u.mem_write(batch+0x10,w(planes,records))
    u.mem_write(planes,b'\x35'*64);u.mem_write(planes+index*16,f(plane));u.mem_write(records,b'\x53'*32);u.mem_write(token,struct.pack('<3hH',*ids,0x2020))
    for n,v in zip(ids,triangle):u.mem_write(vertices+n*12,f(v))
    u.mem_write(query,b'\xa5'*104);u.mem_write(query+0x50,f(start+delta));u.mem_write(query+0x48,f([radius]));u.mem_write(hit,initial)
    untouched=bytes(u.mem_read(batch,0x5000))
    u.mem_write(stack,w(stop,0,batch,index,query,hit,two));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x54de40,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
    accepted=u.reg_read(UC_X86_REG_EAX)&255;expected=w(accepted)+bytes(u.mem_read(hit,32));answers.append(expected);hits+=accepted
    assert bytes(u.mem_read(batch,0x5000))==untouched
    if not accepted:assert expected[4:]==initial
    elif expected[20:32]==f([-v for v in plane[:3]]):flipped+=1
    elif expected[20:32]!=f(plane[:3]):edge_normals+=1
    x.mem_write(b,command+b'\xa5'*16);x.mem_write(stack,w(stop,b+24,b,b+12,struct.unpack('<I',f([radius]))[0],two,b+84));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
    actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+84,32))
    assert actual==expected,('NXDK',case,command.hex(),expected.hex(),actual.hex())
    assert bytes(x.mem_read(b+116,4))==command[116:]
    assert bytes(x.mem_read(b,84))==command[:84] and bytes(x.mem_read(b+120,16))==b'\xa5'*16
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-sphere-triangle'],input=b''.join(commands))
assert actual==b''.join(answers),'PC mismatch'
report=dict(result='PASS',cases=len(commands),hits=hits,flipped_plane_hits=flipped,non_plane_normals=edge_normals,original_sha256=sha,
    scope='Complete54de40 with all plane, edge, fan containment, bounds and vector callees unchanged; steady state; constructor flags preinitialized only. Exact PC/NXDK return and32-byte hit, one/two-sided behavior, strict limit, batch index selection, signed vertex indices resolved to contiguous vertices, preserved query/geometry and rejected output. Original triangle-record pointers represented by caller tokens. Swept triangle including original edge-normal arithmetic; no full part query or XEMU integration.')
(root/'artifacts/model-sphere-triangle.json').write_text(json.dumps(report,indent=2));print(report)

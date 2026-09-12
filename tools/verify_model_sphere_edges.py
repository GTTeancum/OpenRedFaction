"""Full original54dd10 and all geometry helpers versus shared PC/NXDK."""
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
entry=int(re.search(r'\s_rf_collision_model_sphere_edges\s+([0-9a-fA-F]+)',mapping)[1],16)
# Steady-state static constructors only; all query/math callees execute unchanged.
u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
rng=random.Random(0x5076f0);commands=[];answers=[];hits=0;end_hits=0
for case in range(8192):
    count=case%9;start=[rng.uniform(-4,4) for _ in range(3)];delta=[rng.uniform(-6,6) for _ in range(3)];radius=(0,.001,.25,.5,1,2)[case%6]
    verts=[[rng.uniform(-3,3) for _ in range(3)] for _ in range(8)]
    if case%3==0:
        count=4;verts[:4]=[[-2,-2,0],[2,-2,0],[2,2,0],[-2,2,0]]
        start=[rng.uniform(-3,3),rng.uniform(-3,3),2];delta=[0,0,-4]
    if case%17==0:
        count=2;verts[:2]=[[0,-2,0],[0,2,0]];start=[-2,0,0];radius=.5;delta=[1.5,0,0]
    if case%29==0:verts[1]=verts[0][:]
    if case%31==0:delta=[0,0,0]
    if case%37==0:count=-1
    command=f(start+delta+[radius])+w(count)+f([v for row in verts for v in row]+[.123,11,12,13]);assert len(command)==144
    commands.append(command);u.mem_write(b,command);u.mem_write(b+0x1000,w(*(b+32+i*12 for i in range(8))))
    u.mem_write(stack,w(stop,b+132,b,b+12,struct.unpack_from('<I',command,24)[0],count,b+0x1000,b+128));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x5076f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    accepted=u.reg_read(UC_X86_REG_EAX)&255;expected=w(accepted)+bytes(u.mem_read(b+128,16));answers.append(expected);hits+=accepted
    if accepted and struct.unpack_from('<f',expected,4)[0]==1:end_hits+=1
    if not accepted:assert expected[4:]==command[128:]
    assert bytes(u.mem_read(b,128))==command[:128]
    x.mem_write(b,command+b'\xa5'*16);x.mem_write(stack,w(stop,b,b+12,struct.unpack_from('<I',command,24)[0],count,b+32,b+128,b+132));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+128,16))
    assert actual==expected,('NXDK',case,command.hex(),expected.hex(),actual.hex())
    assert bytes(x.mem_read(b,128))==command[:128] and bytes(x.mem_read(b+144,16))==b'\xa5'*16
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-sphere-edges'],input=b''.join(commands))
for case,expected in enumerate(answers):assert actual[case*20:case*20+20]==expected,('PC',case,actual[case*20:case*20+20].hex(),expected.hex())
assert len(actual)==len(answers)*20
report=dict(result='PASS',cases=len(commands),hits=hits,end_hits=end_hits,original_sha256=sha,scope='Complete steady-state5076f0 with all bounds/edge/vector callees unchanged, constructors preinitialized only. Exact original/PC/NXDK hit, fraction, point; ordered closed edges, degenerates, empty/negative count, exact endpoint, preserved misses/inputs. No full swept triangle or live integration.')
(root/'artifacts/model-sphere-edges.json').write_text(json.dumps(report,indent=2));print(report)

"""Original506dd0 model triangle-fan containment against PC/NXDK; no hooks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;pointers=b+0x1000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda v:struct.pack('<'+'f'*len(v),*v)
scalar=lambda bits:struct.unpack('<f',w(bits))[0]
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_model_polygon_contains\s+([0-9a-fA-F]+)',mapping)[1],16)
rng=random.Random(0x506dd0);commands=[];answers=[];hits=0
epsilon=struct.unpack('<I',f([.0001]))[0]
near=[0.,scalar(epsilon-1),scalar(epsilon),scalar(epsilon+1),-scalar(epsilon-1),-scalar(epsilon),-scalar(epsilon+1)]
for case in range(12288):
    count=(3,4,5,8)[case%4];normal=[rng.choice((-2.,-1.,0.,1.,2.)) for _ in range(3)]
    vertices=[[rng.randrange(-64,65)/8 for _ in range(3)] for _ in range(8)];point=[rng.randrange(-64,65)/8 for _ in range(3)]
    if case<8192:
        axis=case%3;a=(axis+1)%3;c=(axis+2)%3;normal=[0.,0.,0.];normal[axis]=(-1.,1.)[case//3%2]
        vertices=[[0.,0.,0.] for _ in range(8)]
        for n in range(1,8):vertices[n][a]=rng.choice(near+[1.,2.,-1.]);vertices[n][c]=rng.choice(near+[1.,2.,-1.])
        # Exact vertices, fan edges, interior and immediately adjacent bounds.
        weight=(0.,.5,1.,scalar(0x3f7fffff),scalar(0x3f800001),-.00001)[case//6%6]
        point=[vertices[1][k]*weight for k in range(3)]
        if case//36%3==1:point=[(vertices[1][k]+vertices[2][k])*.5 for k in range(3)]
        elif case//36%3==2:point=[rng.uniform(-2,2) for _ in range(3)]
        # No coplanarity test: changing the omitted axis must not matter.
        point[axis]=rng.uniform(-100,100)
    command=f(point+normal)+w(count)+f([v for row in vertices for v in row]);commands.append(command)
    pointer_bytes=w(*(b+28+i*12 for i in range(8)));results=[]
    for m,native in ((u,False),(x,True)):
        m.mem_write(b,command);m.mem_write(pointers,pointer_bytes)
        args=(b,count,b+28 if native else pointers,b+12);m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack)
        m.emu_start(entry if native else 0x506dd0,stop,count=10000)
        assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+4
        assert bytes(m.mem_read(b,len(command)))==command and bytes(m.mem_read(pointers,len(pointer_bytes)))==pointer_bytes
        results.append(m.reg_read(UC_X86_REG_EAX)&255)
    assert results[0]==results[1],('NXDK',case,command.hex(),results)
    answers.append(w(results[0]));hits+=results[0]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-polygon'],input=b''.join(commands))
assert actual==b''.join(answers),'PC mismatch'
report=dict(result='PASS',cases=len(commands),hits=hits,misses=len(commands)-hits,original_sha256=sha,
    scope='Full506dd0 with real constructor, no hooks. Exact low-byte PC/NXDK results and unchanged inputs under027f. All projection axes/signs/ties, 3..8 ordered vertices, degenerate fans, edge/vertex points, neighboring epsilon and endpoint float values, off-plane points and irregular polygons. No full model triangle query or XEMU integration.')
(root/'artifacts/model-polygon.json').write_text(json.dumps(report,indent=2));print(report)

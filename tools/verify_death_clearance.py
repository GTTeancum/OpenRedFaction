"""Recover full420d00 clearance behavior with only498e80 ray results supplied."""
import hashlib,json,random,struct,sys,re,subprocess,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xf000;stop=b+0xff00
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(b,65536)
binary=root/'build/xbox/main.exe';xp=pefile.PE(str(binary));xim=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xim)+4095)//4096*4096);x.mem_write(xb,xim);x.mem_map(b,65536)
entry=int(re.search(r'\s_rf_entity_death_clearance\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
callback=b+0xd000;commands=[];expected=[]
trace=[];responses=[]
def hook(m,a,size,data):
    if a not in (0x498e80,callback):return
    sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<5I',m.mem_read(sp,20))
    if a==0x498e80:
        assert args[3:]==(1,0);start,end=args[1:3]
    else:start,end=args[2:4]
    trace.append((bytes(m.mem_read(start,12)),bytes(m.mem_read(end,12))))
    m.reg_write(UC_X86_REG_EAX,responses[len(trace)-1]);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x420d00);counts=[0]*5;allowed=blocked_by_actor=0
matrices=[(1,0,0,0,1,0,0,0,1),(0,0,-1,0,1,0,1,0,0),(-1,0,0,0,1,0,0,0,-1),(0,0,1,0,1,0,-1,0,0)]
for case in range(16384):
    pos=[rng.randrange(-100,101)/4 for _ in range(3)];radius=rng.randrange(1,9)/4;height=rng.randrange(1,17)/4
    matrix=rng.choice(matrices);forward=matrix[6:9];direction=rng.choice((0,1,2,255,256,257));low=direction&255
    if case>=8192:
        rounded=lambda v:struct.unpack('<f',f(v))[0]
        pos=[rounded(rng.uniform(-1000,1000)) for _ in range(3)];radius=rounded(rng.uniform(.01,8));height=rounded(rng.uniform(.01,8))
        angle=rng.uniform(-math.pi,math.pi);c=rounded(math.cos(angle));sn=rounded(math.sin(angle));matrix=(c,0,-sn,0,1,0,sn,0,c);forward=matrix[6:9]
    length=radius*(3 if low==1 else -3);offset=[c*length for c in forward]
    end=[a+c for a,c in zip(pos,offset)];lower=list(pos);lower[1]-=height*.5
    lower_end=[a+c for a,c in zip(lower,offset)];middle=[a+c*.5 for a,c in zip(pos,offset)]
    mid_floor=list(middle);mid_floor[1]-=height+1;end_floor=list(end);end_floor[1]-=height+1
    rays=[(f(*pos),f(*end)),(f(*lower),f(*lower_end)),(f(*middle),f(*mid_floor)),(f(*end),f(*end_floor))]
    responses=[rng.choice((0,1,2,256,257,0xffffffff)) for _ in range(4)]
    if case%2==0:responses=[0,2,1,2]
    body=bytearray(rng.randbytes(0x1500));body[0x3c:0x48]=f(*pos);body[0x48:0x6c]=f(*matrix);body[0x78:0x7c]=f(height);body[0x180:0x184]=f(radius)
    u.mem_write(b,bytes(body));u.mem_write(0x5c95ec,w(b+0x2000))
    actors=[];original_nodes=[]
    for i in range(4):
        address=b+0x2000+i*0x1000;cls=b+0x7000+i*0x100
        other=[pos[j]+rng.randrange(-16,17)/4 for j in range(3)];other_radius=rng.randrange(1,9)/4;flags=rng.choice((0,4,5))
        if case>=8192:other=[rounded(pos[j]+rng.uniform(-16,16)) for j in range(3)];other_radius=rounded(rng.uniform(.01,8))
        node=bytearray(rng.randbytes(0x400));node[0x3c:0x48]=f(*other);node[0x180:0x184]=f(other_radius);node[0x294:0x298]=w(cls)
        node[0x28c:0x290]=w(address+0x1000 if i<3 else 0x5c9360)
        u.mem_write(address,bytes(node));u.mem_write(cls+0x74,w(flags));original_nodes.append((address,bytes(node)));actors.append((other,other_radius,flags))
    count=4;want=1
    for i,response in enumerate(responses):
        if ((response&255)==1 if i<2 else (response&255)==0):count=i+1;want=0;break
    if want:
        for other,other_radius,flags in actors:
            delta=[a-c for a,c in zip(other,pos)];distance=sum(v*v for v in delta);reach=abs(length)+other_radius
            local=[sum(delta[j]*matrix[k*3+j] for j in range(3)) for k in range(3)]
            if flags&4 and distance<=reach*reach and (local[2]>=0 if low==1 else local[2]<=0 if low==0 else True) and abs(local[0])<abs(local[2]):want=0;break
    trace=[];u.mem_write(stack,w(stop,b,direction));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x420d00,stop,count=20000);assert u.reg_read(UC_X86_REG_EIP)==stop
    got=u.reg_read(UC_X86_REG_EAX)&255
    if not got and all(((v&255)!=1 if i<2 else (v&255)!=0) for i,v in enumerate(responses)):blocked_by_actor+=1
    if case<8192:assert trace==rays[:count],(case,'rays',trace,rays[:count])
    if case<8192:assert got==want,(case,'result',got,want,direction,actors)
    assert bytes(u.mem_read(b,len(body)))==body
    for address,node in original_nodes:assert bytes(u.mem_read(address,len(node)))==node
    original_trace=list(trace);wire=f(*pos,*matrix,height,radius);actor_wire=b''.join(f(*a,r)+w(flags) for a,r,flags in actors)
    commands.append(wire+w(direction)+actor_wire+w(*responses));expected.append(w(got,len(trace))+b''.join(a+c for a,c in trace)+bytes((4-len(trace))*24))
    x.mem_write(b,wire);x.mem_write(b+0x2000,actor_wire);x.mem_write(stack,w(stop,b,direction,b+0x2000,4,callback,0));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[]
    x.emu_start(entry,stop,count=20000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert x.reg_read(UC_X86_REG_EAX)==got and trace==original_trace,(case,'NXDK',trace,original_trace)
    assert bytes(x.mem_read(b,len(wire)))==wire and bytes(x.mem_read(b+0x2000,len(actor_wire)))==actor_wire
    counts[len(trace)]+=1;allowed+=got
assert allowed and blocked_by_actor and all(counts[1:])
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--death-clearance'],input=b''.join(commands))==b''.join(expected)
report=dict(nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),result='PASS',cases=16384,ray_counts=counts[1:],allowed=allowed,blocked_by_actor=blocked_by_actor,original_sha256=sha,scope='Full420d00 and all actual vector/distance/transform helpers. Only498e80 ray response boundary supplied. Exact four ray endpoints, flags1/null hit output, early exits and nearby actor filtering against8192 independent dyadic fixtures with four yaw orientations, plus8192 varied finite float fixtures and continuous yaw angles compared directly with original execution. Actor owners unchanged. Exact PC/NXDK clearance results and ray endpoint bytes; source actors unchanged. No live geometry integration; NaN/infinity and overflow behavior is outside the finite-input contract.')
(root/'artifacts/death-clearance.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

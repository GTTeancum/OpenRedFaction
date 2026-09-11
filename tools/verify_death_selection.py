"""Original420c00 death selection versus PC/NXDK, preserving CRT draws."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;tls=b+0x6000;callback=b+0x8000;stack=b+0xe000;stop=b+0xf000
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
entry=int(re.search(r'\s_rf_entity_death_select\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];facts=[0,0];draws=0
def hook(m,a,size,data):
    global draws
    if a not in (0x420d00,0x577eef,callback):return
    sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<3I',m.mem_read(sp,12))
    if a==0x577eef:draws+=1;value=tls
    else:
        direction=args[2];assert direction in (0,1);trace.append(direction);value=facts[direction]
    m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x420c00);commands=[];expected=[];draw_counts=[0,0,0];selections={}
for case in range(8192):
    flags=rng.getrandbits(32)&~0x400
    if case%8==0:flags|=0x400
    damage=[rng.choice((-1,0,6,13)) if case%4==0 else 0 for _ in range(2)]
    action=rng.choice((-1,5,6,7,8,9,11,12,12,14,15,16,44));motions=[rng.choice((-1,-1,0,123)) for _ in range(45)]
    seed=rng.getrandbits(32);facts=[rng.choice((0,1,2,256,0xffffffff)) for _ in range(2)]
    wire=w(flags,*damage,action,*motions);commands.append(wire+w(seed,*facts))
    body=bytearray(rng.randbytes(0x1500));body[0x810:0x814]=w(flags);body[0x824:0x828]=w(action);body[0x138c:0x1394]=w(*damage)
    for i,motion in enumerate(motions):body[0xa54+16*i:0xa58+16*i]=w(motion)
    u.mem_write(b,bytes(body));u.mem_write(tls+20,w(seed));trace=[];draws=0
    selected=run(u,0x420c00,[b]);original_trace=list(trace);random_after=bytes(u.mem_read(tls+20,4));draw_counts[draws]+=1
    assert bytes(u.mem_read(b,len(body)))==body
    x.mem_write(b,wire);x.mem_write(tls,w(seed));x.mem_write(b+0x1000,w(12345));trace=[]
    assert run(x,entry,[b,callback,0,tls,b+0x1000])==0
    assert bytes(x.mem_read(b+0x1000,4))==w(selected) and bytes(x.mem_read(tls,4))==random_after and trace==original_trace,case
    assert bytes(x.mem_read(b,len(wire)))==wire
    expected.append(w(0,selected)+random_after+w(len(trace),trace[0] if trace else -1));selections[str(selected)]=selections.get(str(selected),0)+1
assert all(draw_counts)
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--death-select'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',cases=8192,random_draw_counts=draw_counts,selections=selections,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full original420c00 with real40a130,42a650 and57312d CRT RNG. Only420d00 clearance and577eef TLS boundaries supplied. Exact PC/NXDK selection, RNG state and clearance query order; actor unchanged. No geometry clearance implementation, death motion playback or live lifecycle activation.')
(root/'artifacts/death-selection.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

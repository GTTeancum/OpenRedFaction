"""Original54e000 traversal/preparation versus PC/NXDK; only54daa0 supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;model=b+0x1000;count=model+0x48;query=b+0x2000;hit=b+0x3000;backend=b+0x4000;callback=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_model_parts_query\s+([0-9a-fA-F]+)',mapping)[1],16)
trace=[];replies=[]
def hook(m,address,size,native):
    if address!=(callback if native else 0x54daa0):return
    sp=m.reg_read(UC_X86_REG_ESP);n=5 if native else 4;args=struct.unpack('<'+'I'*n,m.mem_read(sp+4,n*4))
    if native:assert args[0]==0;args=args[1:]
    else:assert m.reg_read(UC_X86_REG_ECX)==model
    part,q,h,reset=args;assert part<4 and q==query and h==hit and reset==0 and len(trace)<4
    trace.append(w(part,reset)+bytes(m.mem_read(query,80))+bytes(m.mem_read(hit,32))+bytes(m.mem_read(count,4)))
    r=replies[part];m.mem_write(count,r[4:8]);m.mem_write(query+76,r[8:12]);m.mem_write(hit,r[12:44]);m.reg_write(UC_X86_REG_EAX,struct.unpack('<I',r[:4])[0])
    m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+4+(0 if native else 16))
u.hook_add(UC_HOOK_CODE,hook,False);x.hook_add(UC_HOOK_CODE,hook,True)
rng=random.Random(0x54e000);commands=[];answers=[];calls=0;empty=0;prepared=0;early=0
for case in range(4096):
    origin=[rng.randrange(-100,101)/8 for _ in range(3)];matrix=[rng.randrange(-8,9)/8 for _ in range(9)]
    start=[rng.randrange(-100,101)/8 for _ in range(3)];delta=[rng.randrange(-100,101)/8 for _ in range(3)]
    flags=(0,1,2,3,0x80000000)[case%5];reset=(0,1,0x100,0x101,2,255)[case//5%6];initial=(-1,0,1,2,4)[case//30%5]
    q=f(origin+matrix+start+delta+[.25])+w(flags);h=bytes(rng.getrandbits(8) for _ in range(32))
    replies=[w(rng.choice((0,1,2,128,0x100,0x101)),rng.choice((-1,0,1,2,4)),rng.choice((0,1,2,3,0x80000000)))+bytes(rng.getrandbits(8) for _ in range(32)) for _ in range(4)]
    commands.append(q+h+w(reset,initial)+b''.join(replies));prepared+=not flags&2;empty+=initial<=0
    outputs=[]
    for m,native in ((u,False),(x,True)):
        m.mem_write(query,q+b'\x5a'*16);m.mem_write(hit,h+b'\x35'*16);m.mem_write(model,b'\xa5'*0x80);m.mem_write(count,w(initial));m.mem_write(backend,w(callback,0));trace.clear()
        args=(count,query,hit,reset,backend) if native else (query,hit,reset)
        m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_ECX,model)
        m.emu_start(entry if native else 0x54e000,stop,count=10000)
        assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+(4 if native else 16)
        assert bytes(m.mem_read(query+80,16))==b'\x5a'*16 and bytes(m.mem_read(hit+32,16))==b'\x35'*16
        original_model=bytearray(b'\xa5'*0x80);original_model[0x48:0x4c]=m.mem_read(count,4);assert bytes(m.mem_read(model,0x80))==original_model
        outputs.append(w(m.reg_read(UC_X86_REG_EAX)&255)+bytes(m.mem_read(query,80))+bytes(m.mem_read(hit,32))+bytes(m.mem_read(count,4))+w(len(trace))+b''.join(trace).ljust(496,b'\0'))
    assert outputs[0]==outputs[1],('NXDK',case)
    answers.append(outputs[0]);calls+=len(trace)
    if trace and struct.unpack('<I',outputs[0][:4])[0] and (struct.unpack('<I',outputs[0][80:84])[0]&1):early+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-parts'],input=b''.join(commands))
assert actual==b''.join(answers),'PC mismatch'
report=dict(result='PASS',cases=len(commands),part_calls=calls,prepared=prepared,initial_empty=empty,early=early,original_sha256=sha,
    scope='Full54e000, real53b7bc and vector transforms; only54daa0 supplied. PC/NXDK final query/result/count, per-call query/result/reset/order and guards match under027f. Mutable part counts/flags/results, non-normalized low-byte OR, exact-reset byte and inverse transforms. Part geometry and native XEMU integration excluded.')
(root/'artifacts/model-parts-query.json').write_text(json.dumps(report,indent=2));print(report)

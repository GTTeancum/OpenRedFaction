"""Full original42f510 versus PC/NXDK, real object lookup, supplied bones/release."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;obj=b+0x1000;emit=b+0x3000;owners=b+0x4000;be=b+0x5000;cb=b+0x6000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,im);m.mem_map(b,65536);return m
def read(m,a,n):return list(struct.unpack('<'+'I'*n,m.mem_read(a,4*n)))
def run(m,a,args):
    m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(a,stop,count=10000)
    assert m.reg_read(UC_X86_REG_EIP)==stop;return m.reg_read(UC_X86_REG_EAX)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary)
entry=int(re.search(r'\s_rf_burn_retarget\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];indices=[];bone_status=0;target=0
def hook(m,a,size,data):
    if a not in (0x42eb20,0x42ed20,cb,cb+16):return
    sp=m.reg_read(UC_X86_REG_ESP);args=read(m,sp,4);result=0
    if a==0x42eb20:
        assert args[1:3]==[b,obj];m.mem_write(b+20,w(*indices));trace[:]=[1,target,read(m,b+16,1)[0],0];result=0 if bone_status else 1
    elif a==cb:
        assert args[2]==target;m.mem_write(args[3],w(*indices));trace[:]=[1,args[2],read(m,b+16,1)[0],0];result=bone_status
    else:
        assert (args[1:3]==[b,0] if a==0x42ed20 else args[2]==3);trace[:]=[0,0,0,3]
    m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x42f510);inputs=[];outputs=[];missing=partial=0
for case in range(4096):
    present=rng.randrange(2);target=rng.choice((0,1023,0x10007,0x80000007));flags=rng.getrandbits(32)
    indices=[rng.choice((-1,0,7,51)) for _ in range(4)];bone_status=-3 if -1 in indices else 0
    old_owners=[rng.getrandbits(32) for _ in range(4)];record=bytearray(rng.randbytes(64));record[:16]=w(11,12,13,14)
    original=bytearray(record);original[:16]=w(*[emit+0x200*i for i in range(4)])
    body=bytearray(rng.randbytes(0x400));body[0x2c:0x30]=w(target);body[0x29c:0x2a0]=w(flags)
    u.mem_write(b,bytes(original));u.mem_write(obj,bytes(body));u.mem_write(0x7394cc,bytes(4096))
    if present:u.mem_write(0x7394cc+4*(target&0xffff),w(obj))
    emitter_bytes=[]
    for i in range(4):
        payload=bytearray(rng.randbytes(0x180));payload[4:8]=w(old_owners[i]);emitter_bytes.append(payload);u.mem_write(emit+0x200*i,bytes(payload))
    trace=[];run(u,0x42f510,[b,target]);want_trace=list(trace)
    final=bytearray(u.mem_read(b,64));final[:16]=record[:16]
    final_flags=read(u,obj+0x29c,1)[0];final_owners=[read(u,emit+0x200*i+4,1)[0] for i in range(4)]
    if present:
        body[0x29c:0x2a0]=w(flags|0x200);partial+=bone_status!=0
        for payload in emitter_bytes:payload[4:8]=w(target)
        assert final[44]==1 and final[45:48]==record[45:48] and final[48:52]==w(0)
        assert final[20:36]==w(*indices)
    else:missing+=1;assert final==record
    assert bytes(u.mem_read(obj,len(body)))==body
    for i,payload in enumerate(emitter_bytes):assert bytes(u.mem_read(emit+0x200*i,len(payload)))==payload
    x.mem_write(b,bytes(record));x.mem_write(obj,w(flags));x.mem_write(emit,w(*old_owners));x.mem_write(owners,w(*[emit+4*i for i in range(4)]));x.mem_write(be,w(cb,cb+16,0));trace=[]
    assert run(x,entry,[b,3,target,obj if present else 0,owners,be])==0
    assert bytes(x.mem_read(b,64))==final and read(x,obj,1)==[final_flags] and read(x,emit,4)==final_owners and trace==want_trace,case
    inputs.append(record+w(target,present,flags,*old_owners,*indices,bone_status))
    outputs.append(w(0)+final+w(final_flags,*final_owners,*want_trace))
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-retarget'],input=b''.join(inputs))==b''.join(outputs)
report=dict(result='PASS',cases=4096,missing_targets=missing,partial_bone_transfers=partial,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full42f510 with real40a0e0 and supplied42eb20/42ed20. Exact normalized PC/NXDK burn bytes, four emitter owner writes, target flags, partial bone retention and callback observations. Actual attachment lookup/release, emitter pool resolution and corpse owner-field transfer remain separate.')
(root/'artifacts/burn-retarget.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

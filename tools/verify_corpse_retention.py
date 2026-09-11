"""Original constructor retention block and all real callees versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX
b=0x30000000;stack=b+0xe000;stop=b+0xf000;out=b+0xd000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary)
entry=int(re.search(r'\s_rf_corpse_retention_apply\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
selected=[]
def observe(m,a,size,data):
    if a==0x4174f0:
        sp=m.reg_read(UC_X86_REG_ESP);pointer=struct.unpack('<I',m.mem_read(sp+4,4))[0];selected.append((pointer-b)//0x400)
u.hook_add(UC_HOOK_CODE,observe)
def shared(head,limit):
    x.mem_write(out,w(0xaaaaaaaa));x.mem_write(stack,w(stop,head,limit,out));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop;return x.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x416da3);inputs=[];outputs=[];total=0;max_fades=0
for case in range(4096):
    count=case%33;rows=[];bodies=[]
    for i in range(count):
        object_flags=rng.getrandbits(32)&~0x4000;flags=rng.getrandbits(32)&~0x43
        if (case//33)%3:
            flags|=rng.choice((0,0,0,1,2,0x40));object_flags|=rng.choice((0,0,0,0x4000))
        created=struct.pack('<f',rng.choice((-1.,0.,1.,2.,3.,4.,100.)));fade=rng.randbytes(4)
        row=w(object_flags,flags)+created+fade;rows.append(row)
        body=bytearray(rng.randbytes(0x400));body[0x7c:0x80]=w(object_flags);body[0x294:0x29c]=created+fade;body[0x29c:0x2a0]=w(flags)
        body[0x28c:0x290]=w(b+(i+1)*0x400 if i+1<count else 0x5cabb8);bodies.append(body)
        u.mem_write(b+i*0x400,bytes(body));x.mem_write(b+i*20,w(b+(i+1)*20 if i+1<count else 0)+row)
    u.mem_write(0x5cae44,w(b if count else 0x5cabb8));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,0);selected=[]
    u.emu_start(0x416da3,0x416dc6,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==0x416dc6 and u.reg_read(UC_X86_REG_ESP)==stack
    assert shared(b if count else 0,count)==0
    assert bytes(x.mem_read(out,4))==w(len(selected));total+=len(selected);max_fades=max(max_fades,len(selected))
    result=[]
    for i,body in enumerate(bodies):
        got=bytes(u.mem_read(b+i*0x400,0x400))
        if i in selected:body[0x298:0x29c]=struct.pack('<f',1);body[0x29c:0x2a0]=w(struct.unpack('<I',rows[i][4:8])[0]|1)
        assert got==body,(case,i)
        row=got[0x7c:0x80]+got[0x29c:0x2a0]+got[0x294:0x29c];result.append(row)
        assert bytes(x.mem_read(b+i*20+4,16))==row,(case,i)
    inputs.append(w(count)+b''.join(rows));outputs.append(w(0,len(selected))+b''.join(result))
assert total and max_fades==27
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-retention'],input=b''.join(inputs))==b''.join(outputs)
# Malformed-list/nonfinite guards must precede every mutation.
for wire,limit in ((w(b,0,0)+struct.pack('<2f',0,7),2),(w(0,0,0)+struct.pack('<2f',float('nan'),7),1),(w(0,0,0)+struct.pack('<2f',0,7),0)):
    x.mem_write(b,wire);assert shared(b,limit)==0xfffffffc
    assert bytes(x.mem_read(b,len(wire)))==wire and bytes(x.mem_read(out,4))==w(0xaaaaaaaa)
report=dict(result='PASS',cases=4096,nxdk_guards=3,fades=total,max_fades=max_fades,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Original416da3..416dc6 constructor retention block with full416f20/416f80/4174f0 and flag predicates, no replaced callees. PC/NXDK final node state and count match for0..32 corpses, protected/fading flags, tied/negative finite times. Three NXDK preflight guards. No creation, fade progression or resource destruction.')
(root/'artifacts/corpse-retention.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

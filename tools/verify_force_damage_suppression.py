"""Complete original impact-suppressing region query vs PC/NXDK, no callee hooks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
u.mem_write(0x1754474,b"\x07");u.mem_write(0x17545a8,bytes(12)) # Initialized OBB scratch; exclude CRT exit registration.
entry=int(re.search(r'\s_rf_physics_force_suppresses_damage\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x45cd20);commands=bytearray();expected=bytearray();hits=[0,0,0,0]
for case in range(1536):
    records=[];count=case%4
    for i in range(3):
        shape=(case//4+i)%5;active=(0,1,255,256,257)[(case//20+i)%5]
        center=(0.,0.,0.) if case<60 else tuple(rng.randrange(-4,5)*.25 for _ in range(3))
        matrix=(0,0,1,0,1,0,-1,0,0) if case%2 else (1,0,0,0,1,0,0,0,1)
        record=w(shape,100+i,0x40 if (case//3+i)%3 else 0)+f(*center,*matrix,1.,-1.,-1.,-1.,1.,1.,1.,2.,2.,2.,10.)+w(active)
        assert len(record)==108;records.append(record)
    if case<9:
        count=1;records[0]=w(case//3+1,100,0x40)+records[0][12:104]+w(1)
    point=(0.,0.,0.) if case%3==0 else (1.,0.,0.) if case%3==1 else (1.0000001192092896,0.,0.)
    if case>=60:point=tuple(rng.randrange(-8,9)*.25 for _ in range(3))
    # Earlier overlapping ordinary regions cannot mask flagged regions;
    # active255 is ignored here even though ordinary force selection accepts it.
    if case in (9,10,11):
        count=3;point=(0.,0.,0.)
        records=[w(2,100+i,0x40 if i or case!=9 else 0)+f(0,0,0,1,0,0,0,1,0,0,0,1,1,-1,-1,-1,1,1,1,2,2,2,10)+w(1) for i in range(3)]
        if case==10:records=[records[i][:104]+w([255,256,257][i]) for i in range(3)]
        if case==11:records=[record[:104]+w(255) for record in records]
    data=b''.join(records);u.mem_write(base,data);u.mem_write(base+0x2000,w(base,base+108,base+216))
    u.mem_write(0x6460bc,w(count,3,base+0x2000));u.mem_write(base+0x1000,f(*point))
    u.mem_write(stack,w(stop,base+0x1000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    try:u.emu_start(0x45ce50,stop,count=100000)
    except Exception as error:raise RuntimeError((case,hex(u.reg_read(UC_X86_REG_EIP)),hex(u.reg_read(UC_X86_REG_ESP)))) from error
    assert u.reg_read(UC_X86_REG_EIP)==stop
    index=u.reg_read(UC_X86_REG_EAX)&255
    assert index in (0,1)
    if case<9:assert index==int(case%3==0 or (case//3>0 and case%3==1)),(case,index)
    if case in (9,10,11):assert index==int(case!=11),(case,index)
    hits[index]+=1
    assert bytes(u.mem_read(base,len(data)))==data
    commands.extend(data+f(*point)+w(count));expected.extend(w(0,index))
    x.mem_write(base,data);x.mem_write(base+0x1000,f(*point));x.mem_write(base+0x3000,w(0xa5a5a5a5))
    x.mem_write(stack,w(stop,base,count,base+0x1000,base+0x3000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x3000,4))==w(0,index),case
    assert bytes(x.mem_read(base,len(data)))==data
# Port finite-domain failures preserve the output. Inactive/unflagged invalid
# geometry is skipped; a later invalid region cannot invalidate an earlier hit.
box=w(2,100,0x40)+f(0,0,0,1,0,0,0,1,0,0,0,1,1,-1,-1,-1,1,1,1,2,2,2,10)+w(1)
bad=w(1,101,0x40)+f(float('nan'),0,0)+box[24:]
guards=[(box+box+box,f(float('nan'),0,0),1,0xfffffffc,0xa5a5a5a5),
        (bad+box+box,f(0,0,0),1,0xfffffffc,0xa5a5a5a5),
        (bad[:104]+w(255)+box+box,f(0,0,0),1,0,0),
        (box+bad+box,f(0,0,0),2,0,1)]
for data,point,count,status,want in guards:
 x.mem_write(base,data);x.mem_write(base+0x1000,point);x.mem_write(base+0x3000,w(0xa5a5a5a5))
 x.mem_write(stack,w(stop,base,count,base+0x1000,base+0x3000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert x.reg_read(UC_X86_REG_EAX)==status and bytes(x.mem_read(base+0x3000,4))==w(want)
 assert bytes(x.mem_read(base,len(data)))==data
 commands.extend(data+point+w(count));expected.extend(w(status,want))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-damage'],input=commands)
assert actual==expected,'PC selection differs'
report=dict(result='PASS',cases=1536,port_contract_cases=4,suppression_counts=hits[:2],original_sha256=digest,
    scope='Complete45ce50 with actual collection, squared-distance, axis-box and oriented-box callees. PC/NXDK suppression matches; records preserved. Exact activation low byte1, flag40, empty/unknown shapes, sphere/box boundaries and overlapping regions. Initialized OBB scratch excludes CRT exit registration. No impact calculation, damage or authored loader.')
(root/'artifacts/force-damage-suppression.json').write_text(json.dumps(report,indent=2));print(report)

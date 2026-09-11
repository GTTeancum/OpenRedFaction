"""Complete original force-region selection vs PC/NXDK, no callee hooks."""
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
entry=int(re.search(r'_rf_physics_force_region_select\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x45cd20);commands=bytearray();expected=bytearray();hits=[0,0,0,0]
for case in range(768):
    records=[];count=case%4
    for i in range(3):
        shape=(case//4+i)%5;active=(0,1,255,256,257)[(case//20+i)%5]
        center=(0.,0.,0.) if case<60 else tuple(rng.randrange(-4,5)*.25 for _ in range(3))
        matrix=(0,0,1,0,1,0,-1,0,0) if case%2 else (1,0,0,0,1,0,0,0,1)
        record=w(shape,100+i,0)+f(*center,*matrix,1.,-1.,-1.,-1.,1.,1.,1.,2.,2.,2.,10.)+w(active)
        assert len(record)==108;records.append(record)
    if case<9:
        count=1;records[0]=w(case//3+1,100,0)+records[0][12:104]+w(1)
    point=(0.,0.,0.) if case%3==0 else (1.,0.,0.) if case%3==1 else (1.0000001192092896,0.,0.)
    if case>=60:point=tuple(rng.randrange(-8,9)*.25 for _ in range(3))
    data=b''.join(records);u.mem_write(base,data);u.mem_write(base+0x2000,w(base,base+108,base+216))
    u.mem_write(0x6460bc,w(count,3,base+0x2000));u.mem_write(base+0x1000,f(*point))
    u.mem_write(stack,w(stop,base+0x1000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    try:u.emu_start(0x45cd20,stop,count=100000)
    except Exception as error:raise RuntimeError((case,hex(u.reg_read(UC_X86_REG_EIP)),hex(u.reg_read(UC_X86_REG_ESP)))) from error
    assert u.reg_read(UC_X86_REG_EIP)==stop
    pointer=u.reg_read(UC_X86_REG_EAX);index=(pointer-base)//108 if pointer else 0xffffffff
    assert pointer==0 or (pointer-base)%108==0 and index<count
    if case<9:assert index==(0 if case%3==0 or (case//3>0 and case%3==1) else 0xffffffff), (case,index)
    hits[3 if index==0xffffffff else index]+=1
    assert bytes(u.mem_read(base,len(data)))==data
    commands.extend(data+f(*point)+w(count));expected.extend(w(0,index))
    x.mem_write(base,data);x.mem_write(base+0x1000,f(*point));x.mem_write(base+0x3000,w(0xa5a5a5a5))
    x.mem_write(stack,w(stop,base,count,base+0x1000,base+0x3000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x3000,4))==w(0,index),case
    assert bytes(x.mem_read(base,len(data)))==data
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-region'],input=commands)
assert actual==expected,'PC selection differs'
report=dict(result='PASS',cases=768,selection_counts=hits,original_sha256=digest,
    scope='Complete45cd20 including collection access, squared-distance, axis-box and oriented-box callees unchanged. PC/NXDK first enabled match agrees, records preserved. Empty lists, unknown shapes, low activation byte, boundaries and overlapping rotated/axis/sphere volumes. Initialized OBB scratch flags supplied to exclude CRT exit registration. No authored reader or force application.')
(root/'artifacts/force-region-select.json').write_text(json.dumps(report,indent=2));print(report)

"""Original recursive portal walk versus shared bounded stack, resolved rectangles."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_room_visibility_lifecycle.py')))
u,base,stack,stop,root,put,call=(c[k] for k in ('u','base','stack','stop','root','put','call'))
x=c['c']['x'];x.mem_map(base+0x10000,0x20000)
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_visibility_traverse\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x4d4860);commands=bytearray();expected=bytearray()
ends=((0,1),(1,2),(2,3),(3,0),(0,2));links=(0,3,4,0,1,1,2,4,2,3);ranges=((0,3),(3,2),(5,3),(8,2))
for case in range(256):
    start=case%4;special=(case//4)%5;flags=1 if case%19==0 else 0;rect=(0.,0.,100.,100.)
    rooms=[];portals=[]
    for i,(first,count) in enumerate(ranges):
        blocked=int(case%11==i);detail=int(case%7==i)
        rooms.extend((first,count,blocked,detail))
        address=base+0x10000+i*384;u.mem_write(address,bytes(384));u.mem_write(address,bytes([blocked]))
        put(address+0x40,detail);put(address+0x164,255);put(address+0x30,count);put(address+0x38,base+0x4000+first*4)
    for i,(a,b) in enumerate(ends):
        rejected=int(case%13==i);box=tuple(float(rng.randint(-25,110)) for _ in range(4)) if case%3 else (10.,10.,90.,90.)
        portal=base+0x20000+i*64;u.mem_write(portal,bytes(64));put(portal,base+0x10000+a*384);put(portal+4,base+0x10000+b*384)
        u.mem_write(portal+0x24,bytes((1,rejected)));u.mem_write(portal+0x28,struct.pack('<4f',*box))
        portals.append(struct.pack('<3I4f',a,b,rejected,*box))
    for i,p in enumerate(links):put(base+0x4000+i*4,base+0x20000+p*64)
    put(0x9bb588,base+0x10000+special*384 if special<4 else 0);put(0x9bb57c,0);u.mem_write(0x9a8548,bytes(16))
    u.mem_write(base+0x5000,struct.pack('<4f',*rect));call(0x4d4860,(0,base+0x10000+start*384,base+0x5000,0,flags,0))
    out=bytearray(4)+u.mem_read(0x9bb57c,4)
    for i in range(4):
        raw=bytes(u.mem_read(base+0x10000+i*384,384));out.extend(struct.pack('<2I',raw[0x160],raw[0x161])+raw[0x164:0x168]+raw[0x16c:0x17c])
    for p in struct.unpack('<4I',u.mem_read(0x9a8548,16)):out.extend(struct.pack('<I',(p-base-0x10000)//384 if p else 0))
    expected.extend(out);command=struct.pack('<3I4f16I10I',start,special,flags,*rect,*rooms,*links)+b''.join(portals);commands.extend(command)
    state=base+0x6000;storage=base+0x7000;order=base+0x8000;packet=base+0x9000;scratch=base+0x10000
    x.mem_write(packet,command);x.mem_write(storage,b''.join(struct.pack('<3I4f',0,0,255,0,0,0,0) for _ in range(4)));x.mem_write(order,bytes(16));x.mem_write(state,struct.pack('<4I',storage,order,4,0))
    args=(state,packet+28,packet+92,10,packet+132,5,start,special,flags,packet+12,scratch)
    x.mem_write(stack,struct.pack('<12I',stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(state+12,4))+bytes(x.mem_read(storage,112))+bytes(x.mem_read(order,16))
    assert actual==out,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,out)) if a!=b][:12])
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--visibility-traverse'],input=commands)==expected
report=dict(result='PASS',cases=256,scope='Full original 4d4860 recursion with five precached portal rectangles on cyclic four-room graphs versus PC/NXDK iterative walk; exact visibility, visit flags, depth, rectangles and order. Blocked/detail rooms, special-room bypass, rejected/nonoverlapping portals and traversal-disable flag. Portal projection/cache generation, authored graph loading and native rendering remain excluded.')
(root/'artifacts/visibility-traverse-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

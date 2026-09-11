"""Original burn spread list/filter/range/damage trace; no live campaign claim."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
bits=lambda v:struct.unpack('<I',f(v))[0]
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;owner=b+0x1000;target=b+0x3000;cls=b+0x6000;stack=b+0xe000;tramp=b+0xf000;u.mem_map(b,65536)
u.mem_write(tramp,b'\xd9\x05'+w(tramp+32)+b'\xc3');u.mem_write(tramp+16,b'\xd9\xee\xc3')
predicates=(0x429990,0x427020,0x40a110,0x4290d0);trace=[]
def hook(m,address,size,context):
    if address not in predicates+(0x504e40,0x4892c0):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<9I',m.mem_read(sp,36))
    if address in predicates:
        assert a[1]==target
        trace.append((address,a[1]));result=gates[predicates.index(address)]
    elif address==0x504e40:
        assert a[1:3]==(bits(5),bits(8))
        assert struct.unpack('<I',m.mem_read(target+0x814,4))[0]==initial_flags|0x2000
        trace.append((address,*a[1:3]));m.reg_write(UC_X86_REG_EIP,tramp);return
    else:
        trace.append((address,*a[1:9]));m.reg_write(UC_X86_REG_EIP,tramp+16);return
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
cases=hits=0;wire_cases=[];wire_expected=[]
for gates in itertools.product((0,1,2,256,257),repeat=4):
 for distance_bits in (bits(0),bits(2)-1,bits(2),bits(2)+1,bits(3)):
    distance=struct.unpack('<f',w(distance_bits))[0];health=100.;divisor=6.5;initial_flags=0x800001
    u.mem_write(b+0x10,w(0x76540001));u.mem_write(owner+0x20,w(0x4321));u.mem_write(owner+0x28c,w(target))
    u.mem_write(target+0x28c,w(0x5cb060));u.mem_write(target+0x294,w(cls));u.mem_write(cls+0x44,f(health))
    u.mem_write(target+0x2c,w(0x12340001));u.mem_write(target+0x3c,w(distance_bits,0,0));u.mem_write(target+0x814,w(initial_flags))
    u.mem_write(0x5cb2ec,w(owner));u.mem_write(0x87243c,w(0xabcdef01));u.mem_write(stack+0x48,bytes(12));u.mem_write(tramp+32,f(divisor))
    for reg,value in ((UC_X86_REG_ESP,stack),(UC_X86_REG_ESI,b),(UC_X86_REG_EBX,owner),(UC_X86_REG_FPCW,0x27f)):u.reg_write(reg,value)
    trace=[];u.emu_start(0x42f0f7,0x42f1dc,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x42f1dc
    want=[];eligible=True
    for address,gate in zip(predicates,gates):
        want.append((address,target))
        if gate&255==1:eligible=False;break
    eligible=eligible and distance*distance<=4
    if eligible:
        hits+=1;want.extend([(0x504e40,bits(5),bits(8)),(0x4892c0,0x12340001,bits((health/divisor)*.25),0x76540001,0xabcdef01,4,0,0x4321,0)])
    assert trace==want,(gates,distance,trace,want)
    assert struct.unpack('<I',u.mem_read(target+0x814,4))[0]==initial_flags|(0x2000 if eligible else 0)
    wire_cases.append(w(*gates,distance_bits,0,0,bits(health),initial_flags,0x12340001,0,0,0,0x76540001,0x4321,0xabcdef01,bits(divisor),2,0,0))
    wire_expected.append(w(0,initial_flags|(0x2000 if eligible else 0),len(trace))+b''.join(w(*row).ljust(36,b'\0') for row in trace)+bytes((6-len(trace))*36))
    cases+=1
report=dict(result='PASS',cases=cases,damage_requests=hits,original_sha256=digest,scope='Unchanged42f0f7..42f1dc with real squared-distance helpers4faf00/40a180. Owner self-exclusion, ordered low-byte predicate gates, radius2 boundary, flag-before-random and full damage arguments verified. Predicates/random/damage supplied; stable list, world spine supplied; excludes attachments, timer gate, callback mutation and live integration.')
(root/'artifacts/burn-spread-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

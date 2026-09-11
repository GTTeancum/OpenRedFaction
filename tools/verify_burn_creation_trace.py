"""Original42e910 burn creation: allocation lists, rejection and effect ownership."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;entity=b+0x4000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536)
calls=[];particles=[];descriptor=0
def hook(m,address,size,context):
    global descriptor
    if address not in (0x42f810,0x426fc0,0x40a1e0,0x42cca0,0x5001d0,0x42eb20,0x42f840,0x497ca0,0x434da0,0x5056a0):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<6I',m.mem_read(sp,24));calls.append(address);result=0;cleanup=0
    if address==0x42f810:result=m.reg_read(UC_X86_REG_ECX)
    elif address==0x426fc0:assert a[1]==0x12340001;result=entity if gate!=1 else 0
    elif address==0x40a1e0:assert a[1]==entity;result=0 if gate==2 else 1
    elif address==0x42cca0:assert a[1]==entity;result=1 if gate==3 else 0
    elif address==0x5001d0:assert a[1:3]==(entity+0x2000,0x595f5c);result=1 if gate==4 else 0
    elif address==0x42eb20:
        assert a[1:3]==(b,entity);m.mem_write(b+0x14,w(10,11,12,13));result=0 if gate==5 else 1
    elif address==0x42f840:descriptor=a[1];result=m.reg_read(UC_X86_REG_ECX);cleanup=4
    elif address==0x497ca0:
        assert a[1]==0x12340001 and a[3:6]==(0,0,1),a
        assert descriptor==(b+0x8000 if len(particles)<3 else b+0x9000)
        result=0 if failure_mask&(1<<len(particles)) else 0x45670000+len(particles)
        particles.append(result)
    elif address==0x434da0:assert a[1]==27;result=0xffffffff if failure_mask&16 else 28
    else:
        assert a[1:6]==(0xffffffff if failure_mask&16 else 28,entity+0x3c,0x3f800000,0x173c378,0),a
        result=0xffffffff if failure_mask&32 else 0x56780001
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4+cleanup);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
def ring(nodes):
    for i,node in enumerate(nodes):u.mem_write(node+0x38,w(nodes[(i+1)%len(nodes)],nodes[i-1]))
def verify_ring(head,nodes):
    assert head==(nodes[0] if nodes else 0),(head,nodes)
    for i,node in enumerate(nodes):assert bytes(u.mem_read(node+0x38,8))==w(nodes[(i+1)%len(nodes)],nodes[i-1]),(node,nodes)
cases=0;successes=0
for free_count in range(4):
 for active_count in range(4):
  for gate in range(6):
   for failure_mask in (0,1,15,16,32,63):
    cases+=1;free=[b+i*64 for i in range(free_count)];active=[b+0x1000+i*64 for i in range(active_count)]
    u.mem_write(b,bytes([0xa5])*0x2000);ring(free);ring(active)
    u.mem_write(0x62f76c,w(free[0] if free else 0));u.mem_write(0x62f770,w(active[0] if active else 0))
    u.mem_write(entity+0x2c,w(0x12340001));u.mem_write(entity+0x294,w(entity+0x2000));u.mem_write(0x595f28,w(0,1));u.mem_write(0x7b2770,w(b+0x8000,b+0x9000));u.mem_write(0x595f24,w(27))
    before=bytes(u.mem_read(b,0x2000));calls=[];particles=[];descriptor=0
    u.mem_write(stack,w(stop,0x12340001,0x23450002));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x42e910,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    success=free_count>0 and gate==0
    expected_calls=[0x42f810]
    if free_count:
        sequence=[0x426fc0,0x40a1e0,0x42cca0,0x5001d0,0x42eb20]
        expected_calls+=sequence[:gate] if gate else sequence
    if success:
        successes+=1;expected_calls += [0x42f840,0x497ca0,0x497ca0,0x497ca0,0x42f840,0x497ca0,0x434da0,0x5056a0]
        want=bytearray(before[:64]);want[:20]=w(*particles,0x12340001);want[20:36]=w(10,11,12,13);want[36:44]=w(0xffffffff if failure_mask&32 else 0x56780001,0x3f800000)
        want[44]=0;want[48:56]=w(0,0x23450002)
        assert bytes(u.mem_read(b,56))==want[:56],cases
        verify_ring(struct.unpack('<I',u.mem_read(0x62f76c,4))[0],free[1:])
        verify_ring(struct.unpack('<I',u.mem_read(0x62f770,4))[0],active+[b] if active else [b])
    else:
        want=bytearray(before)
        if free_count and gate==5:want[20:36]=w(-1,-1,-1,-1)
        assert bytes(u.mem_read(b,0x2000))==want,cases
        verify_ring(struct.unpack('<I',u.mem_read(0x62f76c,4))[0],free)
        verify_ring(struct.unpack('<I',u.mem_read(0x62f770,4))[0],active)
    assert calls==expected_calls,(cases,calls,expected_calls)
    assert u.reg_read(UC_X86_REG_EAX)==(b if success else 0),cases
report=dict(result='PASS',cases=cases,successful_allocations=successes,original_sha256=digest,scope='Complete42e910 with supplied descriptor setup/copy, target/type/immunity/class/bone predicates, particle allocation and audio. Exact rejection order, slot fields, circular free/active lists and return; free/active counts0..3, partial particle and audio failures. No bone resolver, actual particle/audio implementation, pool initialization or cleanup.')
(root/'artifacts/burn-creation-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

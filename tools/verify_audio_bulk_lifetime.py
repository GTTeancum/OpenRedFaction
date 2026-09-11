"""Execute original bulk stop/release ordering and sample reference decrement."""
import itertools,json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
e=runpy.run_path(str(root/'tools/verify_audio_registration.py'));m,u=e['m'],e['u']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
trace=[]
def boundary(machine,address,size,user):
    sp=machine.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',machine.mem_read(sp,8))
    trace.append((address,arg));machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,ret)
for address in (0x521930,0x521a60):m.hook_add(UC_HOOK_CODE,boundary,begin=address,end=address)
def call(address,arg):
    m.mem_write(e['stack'],u(e['stop'],arg&0xffffffff));m.reg_write(UC_X86_REG_ESP,e['stack'])
    m.emu_start(address,e['stop'],count=1000000);assert m.reg_read(UC_X86_REG_EIP)==e['stop']
refs=0
for index,count in itertools.product((-1,-2,0,3),(0,1,2,5,0x7fffffff,0x80000000,0xffffffff)):
    m.mem_write(0x18875d4,u(count));m.mem_write(0x18875d4+3*0x250,u(count));m.mem_write(0x1aed35c,u(100));trace.clear()
    call(0x522270,index);after=(count-1)&0xffffffff
    for slot in (0,3):assert bytes(m.mem_read(0x18875d4+slot*0x250,4))==u(after if index==slot else count)
    freed=index>=0 and after==0
    assert trace==([(0x521a60,index)] if freed else [])
    assert bytes(m.mem_read(0x1aed35c,4))==u(99 if freed else 100);refs+=1
cases=0
for enabled,backend,keep in itertools.product((0,1,256,257),(0,1,2),(0,1,256,257)):
    bank=bytearray(2600*64)
    for slot in range(2600):bank[slot*64+48:slot*64+52]=u(0xffffffff)
    counts=(1,2,0,5)
    for slot,count in enumerate(counts):
        bank[slot*64:slot*64+4]=b'pcm\0';bank[slot*64+48:slot*64+52]=u(slot);bank[slot*64+61]=1
        bank[slot*64+63]=int(slot==3)
        m.mem_write(0x18875d4+slot*0x250,u(count))
    expected=bytearray(bank);want=[];active=enabled&255;release=active and not keep&255
    if active and backend==1:want.extend((0x521930,i) for i in range(30))
    if release:
        for slot in range(3):expected[slot*64+48:slot*64+52]=u(0xffffffff);expected[slot*64+61]=0
        if backend==1:want.append((0x521a60,0))
    m.mem_write(0x1cd3ba8,bytes(bank));m.mem_write(0x1cfc5d0,u(enabled,backend));m.mem_write(0x1cfc5cc,u(4));m.mem_write(0x1aed35c,u(4));trace.clear()
    call(0x5439b0,keep)
    assert trace==want,(enabled,backend,keep,trace,want)
    assert bytes(m.mem_read(0x1cd3ba8,len(bank)))==expected
    for slot,count in enumerate(counts):
        changed=release and backend==1 and slot<3
        assert bytes(m.mem_read(0x18875d4+slot*0x250,4))==u((count-int(bool(changed)))&0xffffffff)
    assert bytes(m.mem_read(0x1aed35c,4))==u(3 if release and backend==1 else 4)
    assert bytes(m.mem_read(0x1cfc5cc,4))==u(4);cases+=1
report=dict(result='PASS',reference_cases=refs,bulk_cases=cases,original_sha256=e['digest'],scope='Original5439b0/544310/522d10/543980/543930/522270 execute; only final voice destruction521930 and resource destruction521a60 supplied. Exact ordered30 voice releases before zero-reference resource destruction, low-byte gates, backend selection, retained sample, full2600 registry bytes, reference underflow/wrap and live-resource count. No destructor internals, counter increment/transition timing or port ownership equivalence.')
(root/'artifacts/audio-bulk-lifetime.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

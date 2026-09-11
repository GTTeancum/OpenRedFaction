"""Original4096-record reset and resource-free cache lifecycle."""
import json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
e=runpy.run_path(str(root/'tools/verify_audio_registration.py'));m,u=e['m'],e['u']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x1887388;size=4096*592

def call(address,*args):
    m.mem_write(e['stack'],u(e['stop'],*args));m.reg_write(UC_X86_REG_ESP,e['stack'])
    m.emu_start(address,e['stop'],count=1000000);assert m.reg_read(UC_X86_REG_EIP)==e['stop']
    return m.reg_read(UC_X86_REG_EAX)
for seed,count in ((0,0),(17,9),(255,4096)):
    bank=bytearray((i+seed)&255 for i in range(size));expected=bytearray(bank)
    for slot in range(4096):
        start=slot*592
        for offset in (524,528,536,544,588,512):struct.pack_into('<I',expected,start+offset,0)
        struct.pack_into('<I',expected,start+516,0xffffffff);expected[start]=expected[start+256]=0
    m.mem_write(base-4,b'HEAD'+bytes(bank)+b'TAIL');m.mem_write(0x1aed35c,u(count));call(0x5215f0)
    assert bytes(m.mem_read(base-4,size+8))==b'HEAD'+expected+b'TAIL'
    assert bytes(m.mem_read(0x1aed35c,4))==u(count)
loads=[]
def load(machine,address,size,user):
    sp=machine.reg_read(UC_X86_REG_ESP);ret,index,mode=struct.unpack('<3I',machine.mem_read(sp,12));loads.append((index,mode))
    machine.reg_write(UC_X86_REG_EAX,0);machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,ret)
m.hook_add(UC_HOOK_CODE,load,begin=0x521d30,end=0x521d30)
def reset():
    m.mem_write(base,bytes(size));m.mem_write(0x1d4f110,bytes(16*28));m.mem_write(0x1aed35c,u(0));call(0x5215f0);loads.clear()
def acquire(name):
    m.mem_write(e['base'],name+b'\0');m.mem_write(e['base']+1024,b'fixture\0')
    return call(0x521c10,e['base'],e['base']+1024,0,0,0)
def word(address):return struct.unpack('<I',m.mem_read(address,4))[0]
reset();assert acquire(b'a.wav')==0 and acquire(b'a.wav')==0
assert word(base+588)==2 and word(0x1aed35c)==1 and len(loads)==1
call(0x522270,0);assert word(base+588)==1 and word(0x1aed35c)==1
call(0x522270,0);assert word(base+588)==0 and word(0x1aed35c)==0 and word(base+516)==0xffffffff
assert bytes(m.mem_read(base,1))==b'\0' and acquire(b'a.wav')==0 and word(base+588)==1
# Observe the original count-based hole behavior, without pretending supplied
# resource-free records prove safe disposal of live DirectSound objects.
reset();assert [acquire(n) for n in (b'a.wav',b'b.wav',b'c.wav')]==[0,1,2]
call(0x522270,0);assert word(0x1aed35c)==2
assert acquire(b'c.wav')==2 and len(loads)==4 and word(base+2*592+588)==2
report=dict(result='PASS',reset_cases=3,records_per_reset=4096,lifecycle_sequences=2,original_sha256=e['digest'],scope='Original5215f0 exact whole-cache reset with canaries and preserved global count. Original acquisition522210/521c10 and release522270/521a60 run together with only resource loading521d30 supplied as success without resources. Duplicate acquire, final release, empty-slot reacquire and lower-slot release hiding a live tail from lookup. Destructor external-resource branches are not covered. Count-based cache is not a proven arbitrary-eviction policy.')
(root/'artifacts/audio-cache-reset.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

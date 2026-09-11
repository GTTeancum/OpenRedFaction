"""Original device sample cache lookup/acquisition, resource loading supplied."""
import itertools,json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
e=runpy.run_path(str(root/'tools/verify_audio_registration.py'));m,u=e['m'],e['u']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x1887388;stride=592;trace=[];load_result=0
def boundary(machine,address,size,user):
    sp=machine.reg_read(UC_X86_REG_ESP);ret,index,mode=struct.unpack('<3I',machine.mem_read(sp,12))
    trace.append((index,mode));machine.reg_write(UC_X86_REG_EAX,load_result&0xffffffff)
    machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,ret)
m.hook_add(UC_HOOK_CODE,boundary,begin=0x521d30,end=0x521d30)
def call(address,*args):
    m.mem_write(e['stack'],u(e['stop'],*args));m.reg_write(UC_X86_REG_ESP,e['stack'])
    m.emu_start(address,e['stop'],count=1000000);assert m.reg_read(UC_X86_REG_EIP)==e['stop']
    return m.reg_read(UC_X86_REG_EAX)
def checksum(name):
    h=0
    for c in name:
        h=(((h<<6)|(h>>26))^(c if c<128 else c-256))&0xffffffff
    return h
names=(b'',b'DoorOpen_07.wav',b'dooropen_07.wav',b'abc',b'abc/def.wav',b'abc\\def.wav',bytes([128,255,65]))
for name in names:
    m.mem_write(e['base'],name+b'\0');assert call(0x5221e0,e['base'])==checksum(name)
assert call(0x5221e0,0)==0xffffffff
cases=0
for count,variant,load_result,mode in itertools.product((0,1,4),range(4),(-1,0),(0,1,257)):
    name=(b'DoorOpen_07.wav',b'dooropen_07.wav',b'missing.wav',b'DoorOpen_07.wav')[variant]
    bank=bytearray([0x5a])*(stride*6)
    for slot in range(6):
        start=slot*stride;stored=b'DoorOpen_07.wav' if slot in (0,3) else b'other.wav'
        bank[start:start+len(stored)+1]=stored+b'\0'
        h=checksum(stored)
        if variant==3 and slot==0:h=0xffffffff
        bank[start+516:start+520]=u(h);bank[start+588:start+592]=u(slot+1)
    expected=bytearray(bank);match=-1
    for slot in range(count):
        start=slot*stride;stored=bytes(bank[start:start+256]).split(b'\0')[0]
        if struct.unpack_from('<I',bank,start+516)[0]==checksum(name) and stored.lower()==name.lower():match=slot;break
    new=match<0;index=count if new else match;start=index*stride
    if new:
        path=b'archive/sounds.vpp';expected[start:start+len(name)+1]=name+b'\0';expected[start+256:start+256+len(path)+1]=path+b'\0'
        expected[start+512:start+516]=u(123);expected[start+520:start+524]=u(7)
        if load_result==-1:expected[start]=expected[start+256]=0
        else:expected[start+516:start+520]=u(checksum(name))
    if not new or load_result!=-1:expected[start+588:start+592]=u(index+2)
    m.mem_write(base,bytes(bank));m.mem_write(0x1aed35c,u(count));m.mem_write(e['base'],name+b'\0');m.mem_write(e['base']+1024,b'archive/sounds.vpp\0');trace.clear()
    result=call(0x521c10,e['base'],e['base']+1024,123,7,mode)
    assert result==(0xffffffff if new and load_result==-1 else index),(count,variant,load_result,mode,result,index)
    assert trace==([(count,mode)] if new else [])
    assert bytes(m.mem_read(base,len(bank)))==expected,(count,variant,load_result,mode)
    assert bytes(m.mem_read(0x1aed35c,4))==u(count+int(new and load_result!=-1));cases+=1
report=dict(result='PASS',hash_cases=len(names)+1,acquisition_cases=cases,original_sha256=e['digest'],scope='Original521c10/522210/5221e0 and57c130 execute; only521d30 resource loading supplied. Full six592-byte records and count checked for duplicate first-match reuse, case-sensitive hash gate, missing/stale entries, failure and mode forwarding. Hash checks include signed high bytes and null. No loading/destruction internals, hole compaction or port cache implementation.')
(root/'artifacts/audio-acquisition.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Original explicit stop dispatch, with final buffer release supplied."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;m.mem_map(base,65536)
calls=[]
def release(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP);ret,index=struct.unpack('<II',machine.mem_read(sp,8));calls.append(index)
    machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,ret)
m.hook_add(UC_HOOK_CODE,release,begin=0x521930,end=0x521930)
cases=0;released=0
for enabled in (0,1,2,255,256,257):
 for backend in (0,1,2):
  for target in (-2147483648,-2,-1,0,1,29,30,54,99):
   for duplicate in (False,True):
    records=bytearray(55*44)
    handles=list(range(55))
    if duplicate:handles[0]=29
    for i,handle in enumerate(handles):records[i*44+16:i*44+20]=w(handle)
    m.mem_write(0x1ad7520,bytes(records));m.mem_write(0x1cfc5d0,bytes([enabled&255]));m.mem_write(0x1cfc5d4,w(backend))
    m.mem_write(stack,w(stop,target));m.reg_write(UC_X86_REG_ESP,stack);calls.clear()
    m.emu_start(0x5442b0,stop,count=10000)
    assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+4
    expected=[handles[:30].index(target)] if enabled&255 and backend==1 and target>=0 and target in handles[:30] else []
    assert calls==expected,(enabled,backend,target,duplicate,calls,expected)
    assert bytes(m.mem_read(0x1ad7520,len(records)))==records
    cases+=1;released+=bool(calls)
report=dict(result='PASS',cases=cases,releases=released,original_sha256=digest,
            scope='Unhooked5442b0,522a20,522f30 dispatch to supplied521930 release. Low-byte gate, backend selector, negative/stale handles, first duplicate, ordinary30-slot boundary. Does not execute release internals or compare platform handle formats.')
(root/'artifacts/audio-stop-dispatch.json').write_text(json.dumps(report,indent=2));print(report)

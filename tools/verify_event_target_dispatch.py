"""Execute original generic event target routing with real handle/list lookup.

Intercept event/mover/auxiliary effects, but execute trigger enable/disable.
This establishes routing evidence; it does not claim shared-code equivalence.
"""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,65536);obj=base+4;aux=base+0x1000;stack=base+0xe000;stop=base+0xf000
def write(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
def hook(cpu,address,size,unused):
 effects={0x4b8b70:('event',3,12),0x46aba0:('mover_on',3,0),0x46b5b0:('mover_off',1,0),0x45b040:('aux_on',1,0),0x45b010:('aux_off',1,0)}
 if address not in effects:return
 name,count,pop=effects[address];sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(sp)
 args=[read(sp+4+i*4) for i in range(count)]
 if name=='event':assert cpu.reg_read(UC_X86_REG_ECX)==base
 trace.append([name,*args]);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);cases=[]
source=0x76543210;actor=0xabcdef01;registered=0x12340000
for kind,fallback,on,suppress,query,flags in itertools.product((0,4,5,6,8),(0,1),(0,1),(0,1,256,257),(registered,0x12350000,0xffffffff),(0,0x10,0xffffffff)):
 u.mem_write(base,bytes(0x2000));u.mem_write(0x7394cc,bytes(4096))
 if kind:
  write(0x7394cc,obj);write(obj+0x24,kind);write(obj+0x2c,registered);write(obj+0x2b0,flags)
 write(0x644ec0,aux if fallback else 0x644ec0)
 write(aux,0x644ec0,0x644ec0,query)
 trace.clear();write(stack,stop,query,source,actor,suppress)
 u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4b65c0 if on else 0x4b6640,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 valid=kind and query==registered;expected=[];expected_flags=flags if kind else 0
 if valid and kind==6:expected=[['event',source if on else actor,actor,on]]
 elif valid and kind==5:expected_flags=(flags&~16) if on else (flags|16)
 elif valid and kind==8 and (on or not(suppress&255)):
  expected=[['mover_on',registered,source,actor]] if on else [['mover_off',registered]]
 elif fallback:expected=[['aux_on' if on else 'aux_off',query]]
 assert trace==expected,(kind,fallback,on,suppress,query,trace,expected)
 assert read(obj+0x2b0)==expected_flags
 cases.append(dict(kind=kind,fallback=fallback,on=on,suppress=suppress,query=query,flags=flags,trace=list(trace),final_flags=expected_flags))
report=dict(result='PASS',cases=len(cases),original_sha256=sha,scope='Original 4b65c0/4b6640, real typed handle lookup, auxiliary linked-list lookup and trigger enable/disable execute unchanged. Event activation, mover and auxiliary effect boundaries intercepted. Registrations synthetic. No shared-code or live campaign equivalence claimed.',results=cases)
(root/'artifacts/event-target-dispatch-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['result'],report['cases'],report['scope'])

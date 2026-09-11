"""Main-model advance dispatch and static frame-phase call-chain evidence."""
import hashlib,json,struct,sys
from pathlib import Path
import capstone,pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
p=pefile.PE(str(root/'Installed_Game/RF.exe'));digest=hashlib.sha256((root/'Installed_Game/RF.exe').read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
outer=list(cs.disasm(p.get_data(0x487a40-0x400000,0x200),0x487a40))
phases=[(i.address,int(i.op_str,16)) for i in outer if i.mnemonic=='call' and i.op_str in ('0x487cf0','0x487e00')]
assert len(phases)==2 and [x[1] for x in phases]==[0x487cf0,0x487e00]
assert struct.unpack('<I',p.get_data(0x487dd4-0x400000,4))[0]==0x487d60
im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0x8000;stop=base+0xf000;wrapper=base+0x2000;instance=base+0x3000
trace=[];predicate=0
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def hook(uc,address,size,context):
 if address not in (0x427020,0x51ba80,0x41daf0,0x4868c0):return
 sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0];pop=0
 trace.append(address)
 if address==0x427020:uc.reg_write(UC_X86_REG_EAX,predicate)
 elif address==0x51ba80:
  assert uc.reg_read(UC_X86_REG_ECX)==instance
  assert bytes(uc.mem_read(sp+4,12))==struct.pack('<fII',1/32,0,1)
  pop=12
 else:assert struct.unpack('<I',uc.mem_read(sp+4,4))[0]==base
 uc.reg_write(UC_X86_REG_ESP,sp+4+pop);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
u.mem_write(base,bytes(0x1500));u.mem_write(base+0x80,w(wrapper));u.mem_write(wrapper,w(2,instance));u.mem_write(0x5a4014,struct.pack('<f',1/32))
rows=[]
for retained_gate in (0,1):
 for predicate in (0,1,2,255):
  trace=[];u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,retained_gate);u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x41dd16,0x41dd49,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x41dd49 and u.reg_read(UC_X86_REG_ESP)==stack
  advanced=0x51ba80 in trace;assert advanced==(bool(retained_gate) or predicate==1)
  rows.append(dict(retained_gate=retained_gate,predicate=predicate,advanced=advanced))
trace=[];u.mem_write(base+0x7c,w(0x20abcd));u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack)
u.emu_start(0x487cf0,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop
assert trace==[0x41daf0,0x4868c0] and struct.unpack('<I',u.mem_read(base+0x7c,4))[0]==(0x20abcd&~0x200000)
report=dict(result='PASS',original_sha256=digest,static_outer_phase_calls=[dict(site=hex(a),target=hex(t)) for a,t in phases],advance_cases=rows,entity_dispatch=trace,scope='Actual487cf0 entity-kind dispatch and prepared41dd16..41dd49 model advance gates, including real503360/501ab0 wrappers;51ba80 intercepted after arguments checked. Static487a40 call sites establish earlier object pass versus later487e00 entity/support pass. Does not execute complete outer frame, upstream retained BL calculation, animation body or footstep consumption.')
(root/'artifacts/npc-animation-order.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

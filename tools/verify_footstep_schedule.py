"""Original entity-update prefix through its footstep scheduling boundary."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import capstone,pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x10000);entity=base;stack=base+0x8000;returned=base+0xf000;boundary=0x41e6e9
cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
ins=list(cs.disasm(p.get_data(0x41e4b0-0x400000,boundary-0x41e4b0),0x41e4b0))
assert ins[-1].mnemonic=='call' and ins[-1].op_str=='0x42f940'
callees={int(i.op_str,16) for i in ins if i.mnemonic=='call'}-{0x40a110}
trace=[];answers={}
def hook(uc,address,size,context):
 if address==boundary:uc.emu_stop();return
 if address not in callees:return
 trace.append(address);sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0]
 uc.reg_write(UC_X86_REG_EAX,answers.get(address,0));uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def put(address,value):u.mem_write(address,struct.pack('<I',value&0xffffffff))
reached=0;mutated=0;cases=0
for excluded,network,player,resolved,model,special,flag40,player_check in itertools.product(range(2),repeat=8):
 trace=[];answers={0x4a3740:resolved,0x4895d0:player_check}
 u.mem_write(entity,bytes(0x1500));put(entity+0x804,-1);put(entity+0x81c,-1);put(entity+0x1400,-1)
 flags=(8 if player else 0)|(0x40 if flag40 else 0)|(0x4000 if excluded else 0);put(entity+0x7c,flags);put(entity+0x34,123);put(entity+0x80,base+0x2000 if model else 0)
 put(0x5afb70,entity if special else 0);u.mem_write(0x64ecb9,bytes([network,0]))
 put(stack,returned);put(stack+4,entity);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x41e4b0,returned,count=100000)
 expected=not excluded and not(network and player and not resolved) and model and not special
 assert (0x42f940 in trace)==bool(expected),(excluded,network,player,resolved,model,special,trace)
 if expected:
  reached+=1
  sequence=[0x421240,0x421170,0x429620,0x41f160,0x421720,0x41f070,0x4194e0,0x4895d0]
  if player_check:sequence += [0x409280,0x409340]
  sequence += [0x42f940]
  observed=[a for a in trace if a in sequence];assert observed==sequence,observed
  assert u.reg_read(UC_X86_REG_EIP)==boundary
 else:assert u.reg_read(UC_X86_REG_EIP)==returned
 value=struct.unpack('<I',u.mem_read(entity+0x34,4))[0]
 assert value==(0 if not excluded and network and flag40 else 123)
 final_flags=struct.unpack('<I',u.mem_read(entity+0x7c,4))[0]
 rejected=not excluded and network and player and not resolved
 assert final_flags==(flags|2 if rejected else flags)
 mutated+=rejected;cases+=1
report=dict(result='PASS',cases=cases,footstep_reached=reached,network_rejections=mutated,original_sha256=digest,scope='Unmodified original41e4b0 prefix through41e6e9. Actual40a110 flag0x4000 predicate executes; other callee bodies supplied at direct-call boundaries; verifies entry gates, flag writes and observed call order under no auxiliary active audio and supplied predicates. Does not establish predicate internals, animation advance ordering, surface writes or execute footsteps.')
(root/'artifacts/footstep-schedule.json').write_text(json.dumps(report,indent=2));print(report)

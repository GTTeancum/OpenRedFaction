"""Full original408ac0 control-flow records for shared AI reconstruction.

Original setters, randomized timer, vector constructor and distance execute;
external AI/lookup decisions are supplied and explicitly recorded.
"""
import hashlib,json,random,struct
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"local/python"))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
root=Path(__file__).resolve().parents[1];exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x10000);inv=b+0x2a0;target=b+0x2000;peers=[b,b+0x4000,b+0x5000,b+0x6000]
thread=b+0x8000;stack=b+0xe000;stop=b+0xf000;fpstub=b+0xf100;threshold=b+0xf200
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
u.mem_write(fpstub,b'\xd9\x05'+w(threshold)+b'\xc3')
# Boundaries are named by source address until their complete semantics are reconstructed.
decisions=(0x427020,0x4174c0,0x4087a0,0x408dc0,0x408ef0,0x427fb0,0x42a020,0x40a110,0x40a210,0x408d90,0x406b70,0x40ac90,0x4062c0)
effects=(0x407ee0,0x4091d0,0x409210,0x408f20,0x4280b0,0x401060,0x4065d0)
trace=[];draws=0;cfg={}
def ret(value):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
def hook(cpu,address,size,context):
 global draws
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+i*4)
 if address==0x577eef:draws+=1;ret(thread);return
 if address==0x426fc0:
  handle=arg(0);assert handle in (55,77);value=(b if cfg['owner'] else 0) if handle==55 else (target if cfg['target'] else 0)
  trace.append([hex(address),handle,value]);ret(value);return
 if address in decisions:
  actor_arg=address in (0x427020,0x427fb0,0x42a020,0x40a110,0x40a210)
  assert arg(0)==(b if actor_arg else 77 if address==0x4174c0 else 55 if address==0x40ac90 else inv),(hex(address),hex(arg(0)))
  value=cfg[hex(address)];trace.append([hex(address),value]);ret(value);return
 if address in effects:
  assert arg(0)==(b if address==0x4280b0 else inv)
  trace.append([hex(address)])
  if address==0x401060:u.mem_write(arg(1),struct.pack('<3f',2,3,4))
  if address==0x4065d0:u.mem_write(inv+0x2b4,w(cfg['changed_state']))
  ret(0);return
 if address==0x401cc0:
  assert arg(0)==inv and arg(1)==1;trace.append([hex(address),cfg['threshold']]);u.reg_write(UC_X86_REG_EIP,fpstub);return
 if address in (0x407e20,0x407e80):
  assert arg(0)==inv;trace.append([hex(address),arg(1)]);return
 if address==0x4fa3b0:
  assert cpu.reg_read(UC_X86_REG_ECX)==inv+0x234 and (arg(0),arg(1))==(2000,4000);trace.append([hex(address)])
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x408ac0);records=[];branches=set();peer_changes=0
for case in range(2048):
 cfg={hex(a):rng.choice((0,1,2,256,257)) for a in decisions}
 cfg.update(owner=True,target=bool(case&1),changed_state=rng.choice((2,3,11)),threshold=rng.choice((0.0,4.0,5.0,6.0)),network=case%4,action=rng.choice((0,2,3,11,17)),disabled=False)
 # Most cases reach the body; first cases systematically exercise the entry gates.
 cfg.update({'0x427020':0,'0x4174c0':0,'0x4087a0':0,'0x408dc0':1})
 if case%16==0:cfg['owner']=False
 elif case%16==1:cfg['disabled']=True
 elif case%16==2:cfg['0x427020']=1
 elif case%16==3:cfg['0x4174c0']=1
 elif case%16==4:cfg['0x4087a0']=1
 elif case%16==5:cfg['0x408dc0']=0
 seed=bytearray(rng.randbytes(0x1500));seed[0x2c:0x30]=w(55);seed[0x2a0:0x2a4]=w(b);seed[0x560:0x564]=w(77)
 seed[0x520:0x524]=w(cfg['action']);seed[0x7d0:0x7d4]=w(0x40000000 if cfg['disabled'] else 0x0f800000)
 seed[0x3c:0x48]=struct.pack('<3f',0,0,0);seed[0x1f8:0x1fc]=w(7)
 for i,peer in enumerate(peers):
  if i:u.mem_write(peer,bytes(0x900));u.mem_write(peer+0x1f8,w(7 if i<3 else 8));u.mem_write(peer+0x520,w(2 if i%2 else 4))
  if not i:seed[0x28c:0x290]=w(peers[1])
  else:u.mem_write(peer+0x28c,w(peers[i+1] if i+1<len(peers) else 0x5cb060))
 u.mem_write(b,bytes(seed));u.mem_write(target+0x3c,struct.pack('<3f',3,4,0));u.mem_write(0x5cb2ec,w(b))
 u.mem_write(0x6fc4d8,bytes([cfg['network']&1]));u.mem_write(0x64ecb9,bytes([cfg['network']>>1]))
 u.mem_write(0x6460f0,struct.pack('<f',12345.75));u.mem_write(0x5a3ed8,w(12345));u.mem_write(thread+0x14,w(case));u.mem_write(threshold,struct.pack('<f',cfg['threshold']))
 u.mem_write(stack,w(stop,inv));u.reg_write(UC_X86_REG_ESP,stack);trace=[];draws=0
 u.emu_start(0x408ac0,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 actual=bytes(u.mem_read(b,len(seed)));restored=bytearray(actual)
 allowed=(0x4d4,0x520,0x528,0x52c,0x530,0x554,0x55c,0x7d0,0x810,0x834)
 for off in allowed:restored[off:off+4]=seed[off:off+4]
 assert restored==seed,('unexpected write',case)
 timer=any(t[0]=='0x4fa3b0' for t in trace);assert draws==int(timer)
 notices=[r(peer+0x7d0) for peer in peers[1:]];assert notices[2]==0
 assert all(v in (0,0x20000) for v in notices)
 if cfg['network'] or not timer:assert not any(notices)
 if timer:assert 14345<=r(b+0x4d4)<=16345
 expected=bytearray(seed)
 def put(off,value):expected[off:off+4]=w(value)
 def get(off):return struct.unpack_from('<I',expected,off)[0]
 progressed=any(t[0]=='0x408dc0' and t[1]&255 for t in trace)
 if progressed:put(0x810,get(0x810)&0xfdffffff)
 for event in trace:
  name=event[0]
  if name=='0x409210':put(0x834,-1);put(0x810,get(0x810)&0xfffffff7)
  elif name=='0x4fa3b0':put(0x4d4,r(b+0x4d4)) # RNG/timer independently verified; retain exact record.
  elif name=='0x4065d0':put(0x554,cfg['changed_state'])
  elif name=='0x407e20':
   put(0x520,3);put(0x528,12345);put(0x52c,-1);put(0x530,-1)
   if cfg['network']:put(0x7d0,get(0x7d0)&0xf07fffff)
  elif name=='0x407e80':put(0x554,event[1]);put(0x55c,12345)
 assert bytes(expected)==actual,('exact actor footprint',case)
 expected_notice=0x20000 if timer and not cfg['network'] and (cfg['0x40a110']&255)!=1 else 0
 assert notices==[expected_notice,expected_notice,0],('peer filter',case)
 assert r(thread+0x14)==((case*214013+2531011)&0xffffffff if timer else case)
 branches.add(tuple(t[0] for t in trace));peer_changes+=sum(bool(v) for v in notices)
 records.append(dict(case=case,input=cfg,initial_fields={hex(o):struct.unpack_from("<I",seed,o)[0] for o in allowed},trace=trace,fields={hex(o):r(b+o) for o in allowed},notices=notices,rng=r(thread+0x14)))
report=dict(result='PASS',cases=len(records),distinct_paths=len(branches),peer_notifications=peer_changes,scope='Full original408ac0 with actual setters, random timer/RNG, constructor, distance and conversion. Lookup/AI decisions and effects supplied; exact actor footprint, stable peer filtering and RNG advance. Trace oracle only, not shared dispatcher or live NPC behavior.',records=records)
(root/'artifacts/ai-select-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})

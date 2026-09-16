"""Original4a1970 vehicle/turret boarding prerequisite ordering at supplied query boundaries."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_ECX
actor=base+0x2000;host=base+0x4000;player=base+0x6000;info=base+0x7000;predicates=[0x42a130,0x4a7690,0x425250,0x4a9dc0,0x4a7420,0x40a0d0,0x4adb60];events=[];passed=False;values=[0]*7;resolved=True
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def cb(cpu,a,size,_):
 global passed
 if a==0x4a1d52:passed=True;cpu.emu_stop();return
 if a not in predicates+[0x4897d0,0x426fc0,0x4c0100]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4)
 if a==0x4897d0:assert arg(1)==actor;word(arg(2),22);value=host;events.append('target')
 elif a==0x426fc0:assert arg(1)==66;value=host if resolved else 0;events.append('resolve')
 elif a==0x4c0100:events.append('fallback');value=1
 else:
  index=predicates.index(a);events.append(hex(a));value=values[index]
  if index==0:assert arg(1)==host and arg(2)==22
  elif index==1:assert arg(1)==host
  elif index==2:assert arg(1)==actor
  elif index==5:assert cpu.reg_read(UC_X86_REG_ECX)==player+0xb8
  else:assert arg(1)==player
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
configs=list(itertools.product([0,1],repeat=7))+[tuple(2 if i==j else 0 for i in range(7)) for j in range(7)]
for kind,mp,values in itertools.product([1,4],[0,1,2],configs):
 events.clear();passed=False;u.mem_write(actor,bytes(0x2000));u.mem_write(host,bytes(0x2000));u.mem_write(player,bytes(0x1000));u.mem_write(info,bytes(0x1000));word(actor+0x200,0xffffffff);word(host+0x294,info);word(host+0x2c,66);word(info+0x1b4,kind);u.mem_write(0x64ecb9,bytes([mp]));word(0x7c75d4,player)
 u.mem_write(stack,struct.pack('<3I',stop,player,actor));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4a1970,stop,count=10000)
 expected=['target','resolve'];ok=mp!=1
 if ok:
  for i,value in enumerate(values):
   expected.append(hex(predicates[i]))
   if (value==1 if i in [2,6] else value!=0):ok=False;break
 if not ok:expected.append('fallback')
 assert events==expected and passed==ok,(kind,mp,values,events,expected,passed,ok)
 rows.append(dict(use_kind=kind,multiplayer_byte=mp,query_returns=list(values),passed_prerequisites=passed,events=events[:]))
(R/'artifacts/future-vehicles-re/boarding-gates.json').write_text(json.dumps(dict(scope="Original 4a1970 vehicle/turret boarding prerequisite ordering; query services supplied, stopped before commit",exe_sha256=sha,query_addresses=[hex(a) for a in predicates],cases=rows),indent=2)+'\n');print('PASS',len(rows),'original prerequisite cases; fullquery ordering,kind1/4,multiplayer exact1,and exact1/nonzero query distinctions')

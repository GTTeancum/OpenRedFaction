"""Original421310 drill excavation orchestration; pose/collision/CSG services supplied."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
actor=base+0x2000;info=base+0x4000;read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0];f=lambda *v:struct.pack('<'+'f'*len(v),*v)
events=[];bit=0;hitmask=0;cutresult=0;cut=None
services=[0x418e60,0x4df1c0,0x4c8a10,0x5056a0,0x505a40,0x4383c0,0x437570,0x467020]
def cb(cpu,a,size,_):
 global bit,cut
 if a not in services:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4);value=0;pop=0
 if a==0x418e60:
  bit=arg(2);assert arg(1)==actor;cpu.mem_write(arg(3),f(0,0,0));cpu.mem_write(arg(4),f(-1+2*bit,2,3));cpu.mem_write(arg(5),f(1,0,0,0,1,0,0,0,1));events.append('pose')
 elif a==0x4df1c0:
  assert arg(3)==1;word(arg(2),int(bool(hitmask&(1<<bit))));cpu.mem_write(arg(2)+4,f(.5));events.append('query');pop=12
 elif a==0x4c8a10:events.append('impact')
 elif a==0x5056a0:events.append('sound'+str(arg(1)));value=55
 elif a==0x505a40:events.append('stop_sound')
 elif a==0x4383c0:events.append('limit_feedback')
 elif a==0x437570:
  name=bytes(cpu.mem_read(arg(1),32)).split(b'\x00')[0].decode();events.append(name);value=91 if 'double' in name else 90
 elif a==0x467020:
  cut=dict(scale=struct.unpack('<f',struct.pack('<I',arg(1)))[0],handle=arg(2),room=arg(3),position=list(struct.unpack('<3f',cpu.mem_read(arg(4),12))),direction=list(struct.unpack('<3f',cpu.mem_read(arg(5),12))),model=arg(6),flags=arg(7));events.append('cut');value=cutresult
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for bits,hitmask,deadline,history,cutresult in itertools.product([0,1,2],[0,1,2,3],[-1,999,1001],[24,25],[0,1]):
 events.clear();cut=None;u.mem_write(actor,bytes(0x2000));u.mem_write(info,bytes(0x1000));word(actor+0x29c,info);word(info+0x1d4,bits);word(actor+0x2c,66);word(actor,77);word(actor+0x13cc,deadline&0xffffffff);word(actor+0x13d0,0xffffffff);word(actor+0x1470,history);u.mem_write(actor+0x60,f(0,0,1));word(0x5a3ed8,1000)
 u.mem_write(stack,struct.pack('<2I',stop,actor));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x421310,stop,count=30000)
 hits=[i for i in range(bits) if hitmask&(1<<i)];expectedcut=bool(hits) and history<25 and deadline==999
 assert bool(cut)==expectedcut,(bits,hitmask,deadline,history,cut,events)
 if cut:
  assert cut['scale']==10*len(hits) and cut['handle']==66 and cut['room']==77 and cut['model']==(91 if len(hits)==2 else 90) and cut['flags']==0x3e and cut['direction']==[0.,0.,1.]
  expectedpos=[sum(-1+2*i for i in hits)/len(hits),struct.unpack('<f',f(2+struct.unpack('<f',f(.78))[0]))[0],4.75]
  assert cut['position']==expectedpos,(cut,expectedpos)
 assert read(actor+0x1470)==history+int(expectedcut and cutresult)
 expectedtimer=(-1 if not hits or expectedcut else 1750 if history<25 and deadline==-1 else deadline)&0xffffffff
 assert read(actor+0x13cc)==expectedtimer,(bits,hitmask,deadline,history,read(actor+0x13cc),expectedtimer,events)
 assert ('limit_feedback' in events)==bool(hits and history>=25)
 rows.append(dict(bits=bits,hit_mask=hitmask,deadline=deadline,history=history,cut_result=cutresult,cut=cut,events=events[:],after_count=read(actor+0x1470),after_deadline=read(actor+0x13cc)))
(R/'artifacts/future-vehicles-re/drill-excavation.json').write_text(json.dumps(dict(scope="Complete421310 with original arithmetic/timers; supplied bit pose, solid sweep, effects and GeoMod callback",exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original active drill excavation cases')

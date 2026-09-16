"""Original4279d0 exit-position prefix; collision services supplied, commit stopped."""
PROBE_SCOPE = __doc__
from pathlib import Path
import sys
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_FPCW
actor=base+0x2000;host=base+0x4000;info=base+0x6000;events=[];candidate=0;free=0;blocked_by='object';exists=True;commit=False
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def callback(cpu,a,size,_):
 global candidate,commit
 if a==0x427c1e:commit=True;cpu.emu_stop();return
 if a not in [0x426fc0,0x49b900,0x499ed0,0x505560]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);rd=lambda n:read(sp+n*4)
 if a==0x426fc0:assert rd(1)==99;value=host if exists else 0
 elif a==0x49b900:
  point=list(struct.unpack('<3f',cpu.mem_read(rd(3),12)));assert rd(1)==actor and rd(2)==actor+0xe4 and rd(5)==host
  events.append(dict(kind='objects',point=point,ignore_host=rd(5)));value=int(candidate!=free and blocked_by=='object')
  if value:candidate+=1
 elif a==0x499ed0:
  point=list(struct.unpack('<3f',cpu.mem_read(rd(2),12)));assert rd(1)==actor+0xe4 and rd(3)==actor+0x88
  events.append(dict(kind='world',point=point));value=int(candidate!=free);candidate+=1
 else:events.append(dict(kind='blocked_feedback',args=[rd(i) for i in range(1,5)]));value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,rd(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,callback);rows=[]
for up in [1.,-1.]:
 for driller in [0,0x1000]:
  for free in range(6):
   for blocked_by in ['object','world']:
    events.clear();candidate=0;commit=False;exists=True;u.mem_write(actor,bytes(0x1000));u.mem_write(host,bytes(0x1000));u.mem_write(info,bytes(0x1000))
    word(actor+0x200,99);word(host+0x294,info);word(info+0x1b4,1);word(info+0x724,driller)
    u.mem_write(actor+0x3c,f(1,2,3));u.mem_write(actor+0xe4,f(1,9,3));u.mem_write(host+0xe4,f(10,10,10));u.mem_write(host+0xfc,f(1,0,0,0,up,0,0,0,1));u.mem_write(host+0x180,f(4))
    before=bytes(u.mem_read(actor,0x1000));hostbefore=bytes(u.mem_read(host,0x1000));u.mem_write(stack,struct.pack('<II',stop,actor));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4279d0,stop,count=10000)
    assert commit==(free<5);assert bytes(u.mem_read(actor,0x1000))==before and bytes(u.mem_read(host,0x1000))==hostbefore
    radius=2 if up>0 and driller else 4;expected=[[10,10+radius,10],[10-radius,10,10],[10+radius,10,10],[10,10,14],[10,10,6]]
    points=[e['point'] for e in events if e['kind']=='objects'];assert points==expected[:min(free+1,5)],(up,driller,free,points,expected)
    rows.append(dict(up=up,driller=bool(driller),first_free=free if free<5 else None,blocked_by=blocked_by,commit_reached=commit,events=events[:]))
(R/'artifacts/future-vehicles-re/exit-candidates.json').write_text(json.dumps(dict(scope=PROBE_SCOPE,exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original prefix cases; all5 candidate directions,blocked/service order,driller half-radius,inverted up; no mutation beforecommit')

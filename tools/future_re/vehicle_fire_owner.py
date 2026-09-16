"""Original4a5029..4a50a8 vehicle firing owner and jeep gate prefix."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_ESI
actor=base+0x2000;host=base+0x4000;ainfo=base+0x6000;hinfo=base+0x7000
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
events=[];passed=False;has_host=0;dead_actor=0;dead_host=0;gunner=0
def cb(cpu,a,size,_):
 global passed
 if a in [0x4a50a8,0x4a58b8]:passed=a==0x4a50a8;cpu.emu_stop();return
 if a not in [0x426fc0,0x427020,0x42acd0]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4)
 if a==0x426fc0:value=host if has_host else 0
 elif a==0x427020:value=dead_actor if arg(1)==actor else dead_host;events.append(['death',arg(1)])
 else:value=gunner;events.append(['gunner',arg(1)])
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for has_host,kind,weapon,dead_actor,dead_host,jeep,gunner in itertools.product([0,1],[0,1,4,7],[-1,3],[0,1],[0,1],[0,1],[0,1]):
 passed=False;events.clear()
 for addr,n in [(actor,0x2000),(host,0x2000),(ainfo,0x1000),(hinfo,0x1000)]:u.mem_write(addr,bytes(n))
 word(actor+0x200,66);word(actor+0x294,ainfo);word(host+0x294,hinfo);word(hinfo+0x1b4,kind)
 owner=host if has_host and kind in [1,4] else actor
 word(actor+0x2a4,9);word(host+0x2a4,8);word(owner+0x2a4,weapon&0xffffffff);word((hinfo if owner==host else ainfo)+0x724,0x400000 if jeep else 0)
 u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,actor);u.emu_start(0x4a5029,stop,count=10000)
 expected=weapon>=0 and not dead_actor and not(dead_host if owner==host else dead_actor) and(not jeep or gunner)
 assert passed==bool(expected) and u.reg_read(UC_X86_REG_ESI)==owner
 rows.append(dict(host=has_host,use_kind=kind,weapon=weapon,actor_dying=dead_actor,host_dying=dead_host,jeep=jeep,gunner=gunner,owner='host' if owner==host else 'actor',admitted=passed,calls=events[:]))
(R/'artifacts/future-vehicles-re/fire-owner.json').write_text(json.dumps(dict(scope="Original4a5029..4a50a8 owner and jeep admission only; death/occupant queries supplied",exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original firing-owner cases')

"""Original4a1970 linked-host exit gates, stopped at successful detach continuation."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
actor=base+0x2000;host=base+0x4000;local=base+0x6000;other=base+0x7000;events=[];continued=False;detach=False
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def cb(cpu,a,size,_):
 global continued
 if a==0x4a1a00:continued=True;cpu.emu_stop();return
 if a not in [0x426fc0,0x4279d0,0x505560,0x4c0100,0x4c04e0]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4)
 if a==0x426fc0:assert arg(1)==99;events.append('host_lookup');value=host
 elif a==0x4279d0:assert arg(1)==actor;events.append('detach');value=int(detach)
 elif a==0x505560:events.append('feedback');value=0
 elif a==0x4c0100:assert arg(1)==actor and arg(2)==1;events.append('fallback_interaction');value=1
 else:raise AssertionError('fallback2 unexpectedly reached')
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for multiplayer,veto,physical,negative,islocal,detach in itertools.product([False,True],repeat=6):
 events.clear();continued=False;u.mem_write(actor,bytes(0x2000));u.mem_write(host,bytes(0x2000));u.mem_write(local,bytes(0x1000));word(actor+0x200,99);word(host+0x814,0x80 if veto else 0);word(host+0x1a8,0x4000 if physical else 0);word(host+0x1380,0xffffffff if negative else 0);word(0x7c75d4,local);word(local+0x10,0x123);u.mem_write(0x64ecb9,bytes([int(multiplayer)]))
 u.mem_write(stack,struct.pack('<3I',stop,local if islocal else other,actor));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4a1970,stop,count=10000)
 denied=veto or physical and negative;expected=[] if multiplayer else ['host_lookup']+((['feedback'] if islocal else [])+['fallback_interaction'] if denied else ['detach'])
 assert events==expected,(events,expected);assert continued==(not multiplayer and not denied and detach);assert read(local+0x10)==(0x923 if not multiplayer and denied else 0x123)
 rows.append(dict(multiplayer=multiplayer,host814_veto=veto,physics1a8_bit4000=physical,host1380_negative=negative,local_caller=islocal,detach_result=detach,success_continuation=continued,local_player_flags=hex(read(local+0x10)),events=events[:]))
(R/'artifacts/future-vehicles-re/player-exit-gates.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original exitgate cases; SP/veto/physics/timer/localfeedback/detachresult boundaries')

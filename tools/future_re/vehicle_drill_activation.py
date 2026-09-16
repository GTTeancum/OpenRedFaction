"""Original41e9ed..41eab0 Driller update activation, spin and released-input cleanup."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBP
actor=base+0x2000;info=base+0x4000;read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0];f=lambda v:struct.pack('<f',v);f32=lambda v:struct.unpack('<f',f(v))[0]
limit=struct.unpack('<f',struct.pack('<I',0x412fede0))[0];events=[];answers=[]
def cb(cpu,a,size,_):
 if a==0x41eab0:cpu.emu_stop();return
 if a not in [0x41a830,0x421310,0x505a40]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4);value=0
 if a==0x41a830:assert arg(1)==66 and arg(2)==39;value=answers.pop(0);events.append('firing')
 elif a==0x421310:assert arg(1)==actor;events.append('excavate')
 else:assert arg(1)==55;events.append('stop_sound')
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for driller,first,second,dt,speed,sound in itertools.product([0,1],[0,1,2],[0,1,2],[0.,.125],[0.,5.,12.],[-1,55]):
 events.clear();answers=[first,second];u.mem_write(actor,bytes(0x2000));u.mem_write(info,bytes(0x1000));word(actor+0x294,info);word(info+0x724,0x1000 if driller else 0);word(actor+0x2c,66);word(actor+0x2a4,39);word(actor+0x814,0x55);word(actor+0x13cc,123);word(actor+0x13d0,sound&0xffffffff);word(actor+0x1470,25);u.mem_write(actor+0x13c8,f(speed));u.mem_write(actor+0x13c4,f(2.));u.mem_write(0x5a4014,f(dt));u.reg_write(UC_X86_REG_ESI,actor);u.reg_write(UC_X86_REG_EBP,0xffffffff);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x41e9ed,stop,count=10000)
 spd=min(limit,max(0.,f32(speed+dt*limit if first else speed-dt*.5*limit))) if driller else speed
 phase=f32(2.+dt*spd) if driller else 2.
 assert bytes(u.mem_read(actor+0x13c8,4))==f(spd) and bytes(u.mem_read(actor+0x13c4,4))==f(phase)
 expected=[] if not driller else ['firing','firing']+(['excavate'] if second==1 else ['stop_sound'] if sound!=-1 else [])
 assert events==expected and read(actor+0x814)==(0x15 if driller else 0x55) and read(actor+0x1470)==25
 assert read(actor+0x13cc)==(0xffffffff if driller and second!=1 else 123) and read(actor+0x13d0)==(0xffffffff if driller and second!=1 else sound&0xffffffff)
 rows.append(dict(driller=driller,first_firing_result=first,second_firing_result=second,dt=dt,initial_speed=speed,initial_sound=sound,after_speed=spd,after_phase=phase,events=events[:]))
(R/'artifacts/future-vehicles-re/drill-activation.json').write_text(json.dumps(dict(scope="Original41e9ed..41eab0 with actual class/clamp/timer helpers; firing/excavation/sound services supplied",exe_sha256=sha,spin_limit=limit,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original drill activation/spin cases')

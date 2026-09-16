"""Original 4a1970 shield delay and liquid boarding admission; original timestamp/liquid helpers."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_ECX
actor=base+0x2000;host=base+0x4000;player=base+0x6000;info=base+0x7000;room=base+0x8000
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
events=[];admitted=False
predicates=[0x42a130,0x4a7690,0x425250,0x4a9dc0,0x4a7420,0x40a0d0,0x4adb60]
def cb(cpu,a,size,_):
 global admitted
 if a==0x4a1dd8:admitted=True;cpu.emu_stop();return
 if a not in predicates+[0x4897d0,0x426fc0,0x4c0100,0x428f00,0x5231e0]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4)
 if a==0x4897d0:word(arg(2),22);value=host
 elif a==0x426fc0:value=host
 elif a==0x4c0100:events.append('fallback');value=1
 elif a==0x428f00:assert arg(1)==actor;events.append('shield_holster_request');value=0
 elif a==0x5231e0:assert arg(1)==0x5a03c0;events.append('generic');value=0
 else:value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for kind,shield,holstered,water,hasroom,liquid,height,local in itertools.product([1,4],[0,1],[0,1],[0,1],[0,1],[0,1],[14.,15.,16.],[0,1]):
 events.clear();admitted=False
 for addr,n in [(actor,0x2000),(host,0x2000),(player,0x1000),(info,0x1000),(room,0x1000)]:u.mem_write(addr,bytes(n))
 word(actor+0x200,0xffffffff);word(actor+0x2a4,9 if shield else 8);word(actor+0x810,0x800 if holstered else 0);word(0x85cce4,9);word(host+0x294,info);word(host+0x2c,66);word(info+0x1b4,kind);word(info+0x724,0x40 if water else 0);word(host,room if hasroom else 0)
 u.mem_write(host+0x40,struct.pack('<f',height));u.mem_write(room+0xc,struct.pack('<f',10.));u.mem_write(room+0x188,struct.pack('<f',5.));u.mem_write(room+0x184,bytes([liquid]));u.mem_write(0x64ecb9,b'\x00');word(0x7c75d4,player if local else player+0x100);word(0x5a3ed8,1000);word(player+0xbc,0xffffffff)
 u.mem_write(stack,struct.pack('<3I',stop,player,actor));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4a1970,stop,count=10000)
 delay=shield and not holstered;ok=not delay and(not water or(hasroom and liquid and height<=15))
 expected=['shield_holster_request'] if delay else (['fallback'] if not ok else (['generic'] if water and local else []))
 assert admitted==ok and events==expected,(kind,shield,holstered,water,hasroom,liquid,height,local,admitted,events)
 assert read(player+0xbc)==(1500 if delay else 0xffffffff)
 rows.append(dict(kind=kind,shield=shield,holstered=holstered,water_only=water,room=hasroom,liquid=liquid,host_y=height,local=local,admitted=bool(ok),events=events[:],delay_word=read(player+0xbc)))
(R/'artifacts/future-vehicles-re/boarding-liquid.json').write_text(json.dumps(dict(scope="Original4a1970 shield delay/liquid admission before attachment; actual4ce080 and4fa360",exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original shield/liquid admission cases')

"""Original4a6060 input owner routing; writer/lookup predicates supplied, clear helpers actual."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
actor=base+0x2000;host=base+0x4000;player=base+0x6000;camera=base+0x8000;remote=base+0xa000
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
events=[];role='ordinary';invehicle=0

def cb(cpu,a,size,_):
 if a not in [0x426fc0,0x4290d0,0x42acd0,0x42d8b0,0x42a0a0,0x431030,0x430720,0x430c70]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4)
 if a==0x426fc0:value=(actor if role!='missing' else 0) if arg(1)==55 else host
 elif a==0x4290d0:value=invehicle
 elif a==0x42acd0:value=int(role=='gunner')
 elif a in [0x42d8b0,0x42a0a0]:value=0
 else:
  assert arg(1)==player
  entry=dict(address=hex(a),gate14c=read(player+0x14c),gate168=read(player+0x168))
  if a!=0x431030:
   entry['record']=arg(2)
   if a==0x430720 and arg(2):cpu.mem_write(arg(2)+12,f(1,2,3))
   if a==0x430c70 and arg(2):entry['direction']=list(struct.unpack('<3f',cpu.mem_read(arg(2)+12,12)))
  events.append(entry);value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for role,mode,locked,f38,invehicle in itertools.product(['ordinary','driver','gunner','missing'],[0,2,4],[0,1],[0,1],[0,1]):
 events.clear()
 for addr,n in [(actor,0x2000),(host,0x2000),(player,0x2000),(camera,0x1000),(remote,0x2000)]:u.mem_write(addr,bytes(n))
 word(player+0x14,55);word(player+0xc4,camera);word(camera,remote);word(camera+8,mode);word(actor+0x200,66 if role in ['driver','gunner'] else 0xffffffff);word(actor+0x7c,0x135);word(player+0x10,8 if locked else 0);u.mem_write(player+0xf38,bytes([f38]))
 for addr in [actor,host,remote]:u.mem_write(addr+0x714,f(9,9,9))
 u.mem_write(stack,struct.pack('<2I',stop,player));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4a6060,stop,count=10000)
 owner=(actor+0x708 if role!='missing' else 0)
 if mode==4:owner=remote+0x708
 elif mode!=2 and role=='driver':owner=host+0x708
 assert [int(e['address'],16) for e in events]==([0x431030] if mode==2 else [0x430720])+[0x430c70]
 assert events[-1]['record']==owner and events[-1]['gate14c']==0 and events[-1]['gate168']==int(not f38)
 if mode!=2:assert events[0]['record']==owner
 if owner:assert events[-1]['direction']==([0.,0.,0.] if locked or role=='gunner' else ([9.,9.,9.] if mode==2 else [1.,2.,3.]))
 if role!='missing':assert read(actor+0x7c)==(0x135 if role=='gunner' else 0x35)
 expectedgate=int(mode==0 and role in ['driver','gunner'] and invehicle)
 assert events[0]['gate14c']==expectedgate and events[0]['gate168']==expectedgate
 rows.append(dict(role=role,camera_mode=mode,locked=locked,f38=f38,in_vehicle_query=invehicle,events=events[:]))
(R/'artifacts/future-vehicles-re/control-owner.json').write_text(json.dumps(dict(scope="Original4a6060 owner selection and direction clearing; supplied ownership predicates/input writer",exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original input-owner cases')

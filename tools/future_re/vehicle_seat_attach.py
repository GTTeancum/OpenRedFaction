"""Execute complete427240 host seat attachment; IO/audio supplied, actual predicates."""
PROBE_SCOPE = __doc__
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_ECX
host=base+0x1000;actor=base+0x3000;info=base+0x6000;array=base+0x7000;seats=[base+0x8000+i*32 for i in range(3)];events=[];exists=True
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def cb(cpu,a,size,_):
 if a not in [0x40a0e0,0x434d00,0x5056a0]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4)
 if a==0x40a0e0:events.append(dict(kind='lookup',handle=arg(1)));val=actor if exists else 0
 elif a==0x434d00:events.append(dict(kind='sound_select',args=[arg(1),arg(2)]));val=888
 else:events.append(dict(kind='sound_play',args=[arg(i) for i in range(1,6)]));val=0
 cpu.reg_write(UC_X86_REG_EAX,val);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for tags,occupants,tag in [([10,20,30],[-1,-1,-1],99),([10,20,30],[-1,-1,-1],20),([20,20,30],[-1,-1,-1],20),([20,20,30],[-1,18,-1],20),([10,20,30],[-1,17,-1],20)]:
 for exists,player,turret,enabled in itertools.product([False,True],repeat=4):
  events.clear();u.mem_write(host,bytes(0x2000));u.mem_write(actor,bytes(0x2000));u.mem_write(info,bytes(0x1000));word(host+0x294,info);word(info+0x114,456);word(info+0x724,0x2000 if turret else 0);word(host+0x2c,66);word(host+0x8cc,3);word(host+0x8d4,array)
  word(actor+0x200,99);word(actor+0x204,88);word(actor+0x2c,17);word(actor+0x7c,8 if player else 0);word(actor+0x1430,5 if player else 0);word(actor+0x858,0x12345678);u.mem_write(actor+0x144,struct.pack('<6f',1,2,3,4,5,6));u.mem_write(0x62ff90,bytes([int(enabled)]))
  for i,(ptr,t,h) in enumerate(zip(seats,tags,occupants)):word(array+i*4,ptr);word(ptr,t);word(ptr+4,h&0xffffffff)
  u.mem_write(stack,struct.pack('<3I',stop,17,tag));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,host);u.emu_start(0x427240,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  selected=max([i for i,t in enumerate(tags) if t==tag],default=-1);success=selected>=0 and occupants[selected]==-1 and exists;expected=[v&0xffffffff for v in occupants]
  if success:expected[selected]=17
  assert [read(p+4) for p in seats]==expected;assert u.reg_read(UC_X86_REG_EAX)&255==int(success);assert read(actor+0x200)==(66 if success else 99);assert read(actor+0x204)==(tag if success else 88)
  velocity=list(struct.unpack('<6f',u.mem_read(actor+0x144,24)));assert velocity==([0]*6 if success else [1,2,3,4,5,6]);mode=read(actor+0x858);assert mode==((0x62ff90 if enabled else 0x62fe50) if success and player else 0x12345678)
  assert u.mem_read(host+0x720,1)[0]==int(success and player and turret)
  rows.append(dict(tags=tags,occupants=occupants,requested_tag=tag,object_exists=exists,player=player,turret=turret,descriptor10_enabled=enabled,success=success,selected=selected,occupants_after=expected,mode_pointer=hex(mode),events=events[:]))
(R/'artifacts/future-vehicles-re/seat-attach.json').write_text(json.dumps(dict(scope=PROBE_SCOPE,exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'full427240 cases; lasttagmatch,occupied/stale rejection,host+taglinks,velocityreset,turret/player/mode10fallback')

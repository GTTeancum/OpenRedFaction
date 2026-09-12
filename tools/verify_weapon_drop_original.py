"""Full original42ae10 SP weapon-drop oracle with explicit resource boundaries.

Retains original RNG, quantity arithmetic, vector/basis construction, string
matching and state writes. Model pose, item mapping/allocation, collision and
notification are supplied and their order recorded.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v));f=lambda *v:struct.pack('<'+'f'*len(v),*v)
signed=lambda v:v if v<0x80000000 else v-0x100000000
f32=lambda v:struct.unpack('<f',f(v))[0]
binary=root/'Installed_Game/RF.exe';digest=hashlib.sha256(binary.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(binary));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x20000);stack=b+0x1d000;stop=b+0x1e000;item=b+0x6000;tls=b+0x9000
get=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
vec=lambda a:list(struct.unpack('<3f',u.mem_read(a,12)))
def string(a):return bytes(u.mem_read(a,64)).split(b'\0')[0].decode('ascii')
def ret(value=0):
 sp=u.reg_read(UC_X86_REG_ESP);target=get(sp);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_EIP,target)
remote_name=string(0x595db0);replacement_name=string(0x595dc0)
case={};events=[];query=None;creation=None;notified=0

def hook(m,a,n,unused):
 global query,creation,notified
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x577eef:ret(tls);return
 if a==0x418e60:
  assert get(sp+4)==b and get(sp+8)==0;events.append('pose')
  m.mem_write(get(sp+12),f(0,0,0));m.mem_write(get(sp+16),f(*case['pose_position']));m.mem_write(get(sp+20),f(*case['pose_basis']));ret(case['pose_handled'])
 elif a==0x459a90:
  assert signed(get(sp+4))==case['current'];events.append('map');ret(case['mapped_item'])
 elif a==0x459430:
  assert string(get(sp+4))==replacement_name;events.append('remote_replacement');ret(case['replacement_item'])
 elif a==0x4031a0:
  assert get(sp+4)==b+0x2a0 and signed(get(sp+8))==case['current'];events.append('remove')
  # Observe only: execute the real inventory removal, with no active players.
 elif a==0x4df1c0:
  q=get(sp+4);out=get(sp+8);assert get(sp+12)==1
  assert get(q)==0 and bytes(m.mem_read(q+4,12))==bytes(12)
  assert bytes(m.mem_read(q+0x10,36))==f(1,0,0,0,1,0,0,0,1)
  assert bytes(m.mem_read(q+0x4c,8))==f(.1)+w(0x2000) and get(out+4)==0x7f7fffff
  query=dict(start=vec(q+0x34),delta=vec(q+0x40),current=signed(get(b+0x2a4)))
  events.append('query');m.mem_write(out,w(case['hit_count']));m.mem_write(out+8,f(*case['hit_point'],*case['normal']))
  target=get(sp);m.reg_write(UC_X86_REG_ESP,sp+16);m.reg_write(UC_X86_REG_EIP,target)
 elif a==0x459100:
  assert get(get(sp+8))==0 and get(sp+16)==case['handle']
  assert [get(sp+i) for i in (28,32,36)]==[0xffffffff,0,0]
  creation=dict(index=signed(get(sp+4)),quantity=signed(get(sp+12)),position=vec(get(sp+20)),basis=list(struct.unpack('<9f',m.mem_read(get(sp+24),36))))
  events.append('create');m.mem_write(item+0x2bc,w(case['item_flags']));m.mem_write(item+0x80,w(0x12345678))
  m.mem_write(item+0x3c,f(*creation['position']));m.mem_write(item+0xe4,f(*creation['position']));ret(item if case['allocation'] else 0)
 elif a==0x401340:
  assert get(sp+4)==case['notification_owner'];assert creation and signed(get(sp+8))==creation['index'] and vec(get(sp+12))==creation['position']
  notified+=1;events.append('notify');ret()
 elif a==0x503310:
  assert get(sp+4)==0x12345678;events.append('bounds')
  m.mem_write(get(sp+8),f(-1,-2,-3));m.mem_write(get(sp+12),f(case['bound_x'],7,9));ret()
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x42ae10);records=[];counts=dict(gated=0,pose_handled=0,no_mapping=0,miss=0,allocation_failure=0,created=0,notifications=0,removed=0)
for n in range(1024):
 current=rng.choice([-1,0,1,3,7]);reserve=rng.choice([-1,0,1,100,0x7fffffff]);loaded=rng.choice([-1,0,1,100]);default=rng.choice([-2147483648,-1,0,3,4,5,10,100,2147483647])
 case=dict(current=current,excluded=0,flags=0x400000 if n%13==0 else 0,reserve=reserve,loaded=loaded,default_count=default,seed=rng.getrandbits(32),
  handle=rng.getrandbits(32),notification_owner=0x24681357,parameter=rng.choice([0,0,1,2,256,257]),
  position=[f32(rng.uniform(-100,100)) for _ in range(3)],extent=f32(rng.uniform(.1,4)),
  pose_position=[f32(rng.uniform(-100,100)) for _ in range(3)],pose_basis=rng.choice([[1,0,0,0,1,0,0,0,1],[0,0,1,0,1,0,-1,0,0],[-1,0,0,0,1,0,0,0,-1]]),
  pose_handled=1 if n%7==0 else 0,mapped_item=-1 if current<0 or n%11==0 else 2,remote=n%3==0,replacement_item=rng.choice([-1,3]),
  hit_count=rng.choice([-1,0,1,2]),hit_point=[f32(rng.uniform(-100,100)) for _ in range(3)],normal=rng.choice([[0,1,0],[1,0,0],[-1,0,0],[.5,.75,.25],[-.5,.75,-.25]]),
  item_flags=rng.getrandbits(32),allocation=n%5!=0,bound_x=f32(rng.uniform(-2,2)))
 events=[];query=None;creation=None;notified=0
 u.mem_write(b,bytes(0x1c000));u.mem_write(b+0x2a4,w(current));u.mem_write(b+0x1a8,w(case['flags']));u.mem_write(b+0x2c,w(case['handle']));u.mem_write(b+0x144c,w(case['notification_owner']));u.mem_write(b+0x3c,f(*case['position']));u.mem_write(b+0x7c4,f(case['extent']));u.mem_write(b+0x42c,bytes([1])*64)
 u.mem_write(0x7c7634,w(0));u.mem_write(0x872118,w(case['excluded']));u.mem_write(0x64ecb9,b'\0');u.mem_write(0x20852f4,w(0));u.mem_write(tls+0x14,w(case['seed']))
 if current>=0:
  u.mem_write(0x85cd2c+current*0x550,w(0));u.mem_write(0x85cd90+current*0x550,w(default));u.mem_write(b+0x2ac,w(reserve));u.mem_write(b+0x32c+current*4,w(loaded))
 classname=remote_name if case['remote'] else 'rifle';u.mem_write(0x6430a0+2*80,w(len(classname),b+0x10000));u.mem_write(b+0x10000,classname.encode()+b'\0')
 before=bytes(u.mem_read(b,0x1500));u.mem_write(stack,w(stop,b,case['parameter']));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x42ae10,stop,count=50000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=u.reg_read(UC_X86_REG_EAX);assert result in (0,item)
 gated=current==case['excluded'] or case['flags']&0x400000 or (current>=0 and signed((reserve+loaded)&0xffffffff)<=0)
 selected=not gated and not case['pose_handled'] and case['mapped_item']!=-1
 want_seed=(case['seed']*214013+2531011)&0xffffffff if selected else case['seed'];assert get(tls+0x14)==want_seed
 removed=selected and case['parameter']&255!=1
 assert signed(get(b+0x2a4))==(-1 if removed else current)
 after=bytearray(u.mem_read(b,0x1500));after[0x2a4:0x2a8]=before[0x2a4:0x2a8]
 if removed:assert after[0x42c+current]==0;after[0x42c+current]=before[0x42c+current]
 assert after==before
 if query:
  start=case['pose_position'][:] if case['parameter']&255==1 else case['position'][:]
  y=start[1];end=f32(y-case['extent']*4)
  if case['parameter']&255==1:start[1]=f32(y+.5)
  assert query['start']==start and query['delta']==[0,f32(end-start[1]),0]
  assert query['current']==(-1 if removed else current)
 if creation:
  draw=(want_seed>>16)&0x7fff;reduction=abs(default)*draw*13421773//(1<<41);reduction=-reduction if default<0 else reduction
  amount=max(4,signed((default-reduction)&0xffffffff))
  assert creation['quantity']==amount and creation['position']==case['hit_point']
  assert creation['index']==(case['replacement_item'] if case['remote'] else case['mapped_item'])
  assert notified==int(bool(case['parameter']&255))
  if result:
   point=[f32(v+f32(a*case['bound_x'])) for v,a in zip(case['hit_point'],case['normal'])]
   assert vec(item+0xe4)==point and vec(item+0x3c)==point and get(item+0x2bc)==case['item_flags']|8
  else:assert 'bounds' not in events
 if removed:assert events.index('remove')<events.index('query')
 kind='gated' if gated else 'pose_handled' if case['pose_handled'] else 'no_mapping' if case['mapped_item']==-1 else 'miss' if case['hit_count']<=0 else 'created' if case['allocation'] else 'allocation_failure'
 counts[kind]+=1;counts['removed']+=int(removed);counts['notifications']+=notified
 records.append(dict(input=case,events=events,query=query,creation=creation,result=bool(result),random=want_seed,current_after=signed(get(b+0x2a4)),item_after=dict(flags=get(item+0x2bc),position=vec(item+0x3c),base_position=vec(item+0xe4)) if result else None))
report=dict(result='PASS',cases=len(records),branches=counts,original_sha256=digest,remote_name=remote_name,replacement_name=replacement_name,
 scope='Full original42ae10 single-player execution. Real504e40/504db0/57312d RNG and quantity conversion, vector/surface-basis arithmetic, string comparator and current-weapon writes, plus real4031a0 with no active players. Supplied model-pose, mapping, collision, item creation, notification and bounds boundaries. No shared C/NXDK or live gameplay claim.',records=records)
(root/'artifacts/weapon-drop-original.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='records'})

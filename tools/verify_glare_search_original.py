"""Audit full414e00 search order/cache writes; supplied external services."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x20000);G=B;CAM=B+0x1000;MOV=[B+0x2000,B+0x3000];ACT=[B+0x4000,B+0x5000];PLAYER=B+0x6000;S=B+0x1e000;STOP=S+0x1000
identity=(1,0,0,0,1,0,0,0,1);basis=(0,0,1,0,1,0,-1,0,0);camera=(0,0,-4);glare=(0,0,4);pos=(1,2,3)
trace=[];queries=[];cfg={};counts={k:0 for k in ('cached_actor','cached_solid','world','solid','actor','clear')}
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+i*4);result=0;pop=0
 if address==0x40a0e0:
  trace.append(('lookup',arg(0)));result={41:ACT[0],42:ACT[1]}.get(arg(0),0)
 elif address==0x415280:
  assert arg(1)==G and arg(2)==CAM;trace.append(('object',arg(0)));result=cfg['blocks'][ACT.index(arg(0))]
 elif address==0x40a490:
  token=cpu.reg_read(UC_X86_REG_ECX);trace.append(('room',token));result=7 if token==PLAYER else cfg['rooms'][ACT.index(token)]
 elif address==0x4290d0:
  trace.append(('state',arg(0)));result=cfg['player_state'] if arg(0)==PLAYER else cfg['states'][ACT.index(arg(0))]
 elif address==0x426fc0:
  assert arg(0)==73;trace.append(('associated',73));result=cfg['associated']
 elif address==0x4df1c0:
  token=cpu.reg_read(UC_X86_REG_ECX);assert arg(2)==1;trace.append(('solid',token));queries.append((token,bytes(cpu.mem_read(arg(0),84))))
  count=cfg['world_hit'] if token==99 else cfg['solid_hits'][token-101]
  cpu.mem_write(arg(1),w(count)+f(1)+bytes(24)+w(0,0x1234));pop=12
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
for a in (0x40a0e0,0x415280,0x40a490,0x4290d0,0x426fc0,0x4df1c0):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def predicted(c):
 t=[];cache=c['cache'];solid=c['cached_solid'];face=c['face']
 def done(visible,why):return visible,(cache,solid,face),t,why
 def obj(a):t.append(('object',a));return bool(c['blocks'][ACT.index(a)]&255)
 def solidtest(a):
  i=MOV.index(a)
  if c['solid_flags'][i]&16 and not c['outside'][i]:t.append(('solid',101+i));return c['solid_hits'][i]>0
  return False
 if cache!=0xffffffff:
  t.append(('lookup',cache));a={41:ACT[0],42:ACT[1]}.get(cache)
  if a and obj(a):return done(0,'cached_actor')
  cache=0xffffffff
 if solid:
  if solidtest(solid):return done(0,'cached_solid')
  solid=0
 t.append(('solid',99))
 if c['world_hit']>0:face=0x1234;return done(0,'world')
 for a in MOV[:c['mover_count']]:
  if solidtest(a):solid=a;return done(0,'solid')
 if c['player']:
  t.append(('room',PLAYER))
  for i,a in enumerate(ACT[:c['actor_count']]):
   t.append(('room',a))
   if c['rooms'][i]!=7:continue
   t.append(('state',a))
   if c['states'][i]&255:continue
   t.append(('state',PLAYER))
   if c['player_state']&255==1:
    t.append(('associated',73))
    if c['associated']==a:continue
   if obj(a):cache=41+i;return done(0,'actor')
 t.append(('lookup',c['parent']));a={41:ACT[0],42:ACT[1]}.get(c['parent'])
 if a and obj(a):cache=41+ACT.index(a);return done(0,'actor')
 return done(1,'clear')
rng=random.Random(41400);cached_poison_checks=0;world_checks=0;mover_checks=0
for case in range(2048):
 cfg=dict(mover_count=rng.randrange(3),actor_count=rng.randrange(3),cache=rng.choice((0xffffffff,41,42,999)),cached_solid=rng.choice((0,*MOV)),face=rng.choice((0,0x4567)),blocks=[rng.choice((0,1,256,257)) for _ in ACT],solid_hits=[rng.choice((-1,0,1,2)) for _ in MOV],world_hit=rng.choice((-1,0,1)),solid_flags=[rng.choice((0,16)) for _ in MOV],outside=[rng.randrange(3)==0 for _ in MOV],player=rng.randrange(2),rooms=[rng.choice((7,8)) for _ in ACT],states=[rng.choice((0,1,256,257)) for _ in ACT],player_state=rng.choice((0,1,2,257)),associated=rng.choice((0,*ACT)),parent=rng.choice((41,42,999)),special=rng.choice((0,1,255)))
 poison=0x3c if case&1 else 0xa5;u.mem_write(S-0x1000,bytes([poison])*0x1000)
 raw=bytearray(b'\xa5'*768);raw[0x30:0x34]=w(cfg['parent']);raw[0x3c:0x48]=f(*glare);raw[0x290:0x29c]=w(cfg['cache'],cfg['cached_solid'],cfg['face']);u.mem_write(G,bytes(raw));u.mem_write(CAM,f(*camera));before={}
 for i,a in enumerate(MOV):
  data=bytearray(b'\xa5'*768);data[0x3c:0x48]=f(*pos);data[0x48:0x6c]=f(*basis);data[0x7c:0x80]=w(cfg['solid_flags'][i]);bounds=(20,20,20,30,30,30) if cfg['outside'][i] else (-10,-10,-10,10,10,10);data[0x190:0x1a8]=f(*bounds);data[0x28c:0x290]=w(MOV[i+1] if i+1<cfg['mover_count'] else 0x64e6e0);data[0x294:0x298]=w(101+i);u.mem_write(a,bytes(data));before[a]=data
 for i,a in enumerate(ACT):
  data=bytearray(b'\xa5'*768);data[0x2c:0x30]=w(41+i);data[0x28c:0x290]=w(ACT[i+1] if i+1<cfg['actor_count'] else 0x5cb060);u.mem_write(a,bytes(data));before[a]=data
 u.mem_write(PLAYER+0x200,w(73));u.mem_write(0x5cb054,w(PLAYER if cfg['player'] else 0));u.mem_write(0x64e96c,w(MOV[0] if cfg['mover_count'] else 0x64e6e0));u.mem_write(0x5cb2ec,w(ACT[0] if cfg['actor_count'] else 0x5cb060));u.mem_write(0x6460e8,w(99));u.mem_write(0x5cab9d,bytes([cfg['special']]))
 trace=[];queries=[];u.mem_write(S,w(STOP,G,CAM));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x414e00,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 visible,caches,expected,why=predicted(cfg);assert trace==expected,(case,trace,expected);assert u.reg_read(UC_X86_REG_EAX)&255==visible;assert bytes(u.mem_read(G+0x290,12))==w(*caches);counts[why]+=1
 raw[0x290:0x29c]=w(*caches);assert bytes(u.mem_read(G,768))==raw
 for a,data in before.items():assert bytes(u.mem_read(a,768))==data
 for index,(token,query) in enumerate(queries):
  preferred=cfg['face'] if any(t==99 for t,_ in queries[:index+1]) else 0
  assert query[:4]==w(preferred)
  if token==99:
   assert query[4:]==f(0,0,0,*identity,*camera,0,0,8,0)+w(0x85 if cfg['special'] else 5);world_checks+=1
  else:
   # Camera/glare translated by (1,2,3), then row-dot 90-degree basis.
   assert query[52:76]==f(-7,-2,1,8,0,0)
   if index==0:
    assert query[4:52]==bytes([poison])*48 and query[76:84]==bytes([poison])*8;cached_poison_checks+=1
   else:assert query[4:52]==f(0,0,0,*identity) and query[76:84]==f(0)+w(0x85 if cfg['special'] else 5)
   mover_checks+=1
assert all(counts.values()) and cached_poison_checks and world_checks and mover_checks
report=dict(result='PASS',cases=2048,outcomes=counts,world_queries=world_checks,mover_queries=mover_checks,uninitialized_cached_queries=cached_poison_checks,original_sha256=sha,scope='Full original414e00 with actual vector/matrix/AABB helpers. Supplied object lookup,415280,room/state/association and solid query services. Ordered calls, low-byte filters, signed hit counts, three cache fields, query inputs and immutable other owner bytes. Cached mover first query retains poisoned stack origin/basis/radius/flags; this is observed original behavior, not a port policy. No shared implementation or native gameplay claim.')
(root/'artifacts/glare-search-original.json').write_text(json.dumps(report,indent=2));print(report)

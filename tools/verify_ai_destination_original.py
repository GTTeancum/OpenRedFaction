"""Full original40ac90 destination orchestration; navigation service boundaries supplied."""
import hashlib,json,random,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x10000);INV=B+0x2a0;POINT=B+0x2000;NODES=[B+0x3000,B+0x3100];TARGET=B+0x4000;STACK=B+0xe000;STOP=B+0xf000;FP=STOP+0x100;LIMIT=STOP+0x200
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
vec=lambda a:list(struct.unpack('<3f',u.mem_read(a,12)))
u.mem_write(FP,b'\xd9\x05'+w(LIMIT)+b'\xc3')
trace=[];cfg={}
def ret(value,pop=0):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i)
 if address==0x426fc0:
  handle=arg(0);assert handle in (55,77);trace.append(['lookup',handle]);ret((B if cfg['present'] else 0) if handle==55 else TARGET if cfg['target'] else 0)
 elif address==0x409210:assert arg(0)==INV;trace.append(['reset']);ret(0)
 elif address==0x40aae0:
  assert arg(0)==B;trace.append(['prepare']);cpu.mem_write(0x5af638,bytes([cfg['mode']]));cpu.mem_write(0x5af634,f(1.5));ret(0)
 elif address==0x4077a0:
  assert arg(0)==INV;trace.append(['limit']);cpu.mem_write(LIMIT,f(cfg['limit']));cpu.reg_write(UC_X86_REG_EIP,FP)
 elif address==0x40c2c0:
  assert arg(0)==POINT and arg(1)==r(B+0x7c0) and arg(2)==r(B+0x7c4) and arg(3)&255==cfg['mode'] and arg(4)==0x5af628 and arg(5)==0x5af62c and arg(6)==0
  trace.append(['nodes',cfg['node_result']]);cpu.mem_write(arg(4),w(NODES[0] if cfg['nodes'] else 0));cpu.mem_write(arg(5),w(NODES[1] if cfg['nodes']==2 else 0));ret(cfg['node_result'])
 elif address==0x40cb40:assert cpu.reg_read(UC_X86_REG_ECX)==B+0x648;trace.append(['clear_nodes']);ret(0)
 elif address==0x45ec40:
  assert cpu.reg_read(UC_X86_REG_ECX)==B+0x648;trace.append(['add_node',NODES.index(arg(0))]);ret(0,4)
 elif address==0x40b0d0:
  assert [arg(i) for i in range(4)]==[B+0x5b0,B,B+0x620,TARGET if cfg['action']==3 and cfg['target'] else 0]
  trace.append(['direct',cfg['direct']]);ret(cfg['direct'])
 elif address==0x4cebd0:
  assert cpu.reg_read(UC_X86_REG_ECX)==TARGET+0x300 and arg(0)==0x5af618
  trace.append(['search',cfg['search'],cfg['count']]);cpu.mem_write(0x5af64c,w(cfg['count']));ret(cfg['search'],4)
 elif address==0x4fa360:
  assert cpu.reg_read(UC_X86_REG_ECX)==B+0x6bc and arg(0)==0;trace.append(['timer']) # Real constructor/timer executes.
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x40ac90);records=[];paths=set();accepted=0;partial=0
allowed_words=[0x588,0x58c,0x590,0x59c,0x5a0,0x660,0x6bc]
allowed_vectors=[0x5a4,0x5b0,0x620,0x62c,0x6d4]
for case in range(2048):
 cfg=dict(present=case%16!=0,action=rng.choice((2,3,16,4)),state=rng.choice((1,3)),mode=rng.choice((0,1,2)),nodes=case%3,node_result=rng.choice((0,1,2,256,257)),limit=rng.choice((0.,1.,100.)),direct=rng.choice((0,1,2,256,257)),search=rng.choice((0,1,2,256,257)),count=rng.choice((0,1,2,7)),target=bool(case&1),coincident=case%8==0)
 seed=bytearray(rng.randbytes(0x1500))
 for off,value in ((0x2c,55),(0x520,cfg['action']),(0x554,cfg['state']),(0x560,77)):seed[off:off+4]=w(value)
 for off,values in ((0x3c,(2,3,4)),(0x7d4,(6,7,8))):seed[off:off+12]=f(*values)
 seed[0x7c0:0x7c8]=f(1,2);u.mem_write(B,bytes(seed));u.mem_write(POINT,f(4,5,6))
 for i,node in enumerate(NODES):
  u.mem_write(node,bytes(0x30));u.mem_write(node+4,f(10));u.mem_write(node+0xc,f(*( (0,8,0) if not i or cfg['coincident'] else (8,12,0))));u.mem_write(node+0x1c,f(4,2))
 u.mem_write(0x5af618,bytes(0x40));u.mem_write(0x6460e8,w(TARGET));u.mem_write(0x5a3ed8,w(12345))
 u.mem_write(STACK,w(STOP,55,POINT));u.reg_write(UC_X86_REG_ESP,STACK);trace=[];u.emu_start(0x40ac90,STOP,count=200000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==STACK+4
 result=u.reg_read(UC_X86_REG_EAX)&255;assert result in (0,1)
 actual=bytes(u.mem_read(B,len(seed)));restored=bytearray(actual)
 for off in allowed_words:restored[off:off+4]=seed[off:off+4]
 for off in allowed_vectors:restored[off:off+12]=seed[off:off+12]
 assert restored==seed,('unexpected actor write',case)
 if not cfg['present']:assert actual==seed and trace==[['lookup',55]] and not result
 else:
  assert trace[:2]==[['lookup',55],['reset']] and vec(B+0x620)==[4,5,6] and r(0x5af624)==B+0x620
  fast=cfg['action']==3 and cfg['state']==3
  if fast:
   assert trace==[['lookup',55],['reset'],['timer']] and result==1
   assert vec(B+0x5a4)==vec(B+0x5b0)==vec(B+0x6d4)==[2,3,4]
  else:
   assert r(B+0x660)==int(cfg['mode']!=1)
   assert trace[2:5]==[['prepare'],['limit'],['nodes',cfg['node_result']]]
   if cfg['mode']!=1 or not cfg['nodes']:assert vec(B+0x62c)==[4,5,6]
   if cfg['mode']==1 and cfg['nodes']==1 and cfg['node_result']&255==1:assert vec(B+0x62c)==[4,10.5,6]
   reached=any(t[0]=='clear_nodes' for t in trace)
   if not reached:assert not result and cfg['action'] not in (2,16)
   else:
    assert [t for t in trace if t[0]=='add_node']==[['add_node',i] for i in range(cfg['nodes'])]
    searched=any(t[0]=='search' for t in trace)
    assert searched==((cfg['direct']&255)!=1)
    want=(cfg['direct']&255)==1 or ((cfg['search']&255)!=0 and cfg['count']>=2)
    assert result==int(want)
    if searched:
     assert r(0x5af63a)&255==1
     assert struct.unpack('<f',u.mem_read(0x5af640,4))[0]==(cfg['limit'] if cfg['action']==3 else 0)
     if cfg['search']&255:assert r(B+0x588)==cfg['count']
   if result:assert vec(B+0x6d4)==[6,7,8]
  if result:
   assert r(B+0x59c)==0 and r(B+0x5a0)==1 and r(B+0x6bc)==12345
   if fast or cfg['direct']&255==1:assert r(B+0x588)==2 and r(B+0x58c)==B+0x5a4 and r(B+0x590)==B+0x620
  if not result and actual!=seed:partial+=1
 accepted+=result;paths.add(tuple(tuple(t) for t in trace))
 records.append(dict(case=case,input=cfg,result=result,trace=trace,initial_words={hex(o):struct.unpack_from('<I',seed,o)[0] for o in allowed_words},initial_vector_bits={hex(o):list(struct.unpack_from('<3I',seed,o)) for o in allowed_vectors},words={hex(o):r(B+o) for o in allowed_words},vectors={hex(o):vec(B+o) for o in allowed_vectors}))
report=dict(result='PASS',cases=len(records),accepted=accepted,partial_failures=partial,distinct_traces=len(paths),scope='Full original40ac90 and actual vector math, projection, normalization, distance and timer. Lookup, animation reset, navigation preparation/node selection/list/direct route/search and scalar limit supplied. Actor footprint and branch publication checked. Trace oracle only; no shared destination or live route execution claim.',records=records)
(root/'artifacts/ai-destination-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})

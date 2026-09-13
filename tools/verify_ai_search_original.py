"""Original4ce8c0 graph search with real scoring/list removal/path recursion.
Only scratch allocation/append/free, visibility and route-output append are supplied.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
f32=lambda v:struct.unpack('<f',f(v))[0]
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);u.mem_map(0,4096)
B=0x30000000;u.mem_map(B,0x10000);Q=B+0x2000;LIST=B+0x3000;OPEN=B+0x4000;ARRAY=B+0x5000;STACK=B+0xe000;STOP=B+0xf000;ALT=B+0x6000
nodes=[B+i*0x100 for i in range(8)];r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
cfg={};trace=[];path=[];open_owner=0

def ret(value=0,pop=0):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
def hook(cpu,address,size,context):
 global open_owner
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);this=cpu.reg_read(UC_X86_REG_ECX)
 if address==0x4d3120:
  assert arg(0)==0;open_owner=this;cpu.mem_write(this,w(0,8,OPEN));trace.append(['init']);ret(this,4)
 elif address==0x45ec40:
  assert this==open_owner;n=r(this);assert n<8;cpu.mem_write(OPEN+4*n,w(arg(0)));cpu.mem_write(this,w(n+1));trace.append(['push',nodes.index(arg(0))]);ret(0,4)
 elif address==0x4ce390:
  assert this==open_owner;trace.append(['pop',nodes.index(r(OPEN+4*arg(0)))]) # Actual stable removal executes.
 elif address==0x40ec50:
  assert this==open_owner;trace.append(['free']);ret()
 elif address==0x4ce740:
  assert arg(0)==0x1234 and arg(4)==0x3f800000;i=nodes.index(arg(1));value=cfg['visible'][i];trace.append(['visible',i,value]);ret(value)
 elif address==0x4ce6c0:
  assert arg(0)==ALT;i=nodes.index(arg(1)-12);j=nodes.index(arg(2)-12);value=cfg['edges'][i][j];trace.append(['edge',i,j,value]);ret(value)
 elif address==0x4cebb0:
  assert arg(0)==Q;path.append(nodes.index(arg(1)));ret()
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x4ce8c0);records=[];successes=0;alternate=0;near=0
for case in range(2048):
 count=1+case%8;cfg=dict(count=count,goal=rng.randrange(count),alternate=bool(case&1),limit=5.0 if case%3==0 else 0.0)
 cfg['positions']=[[float(rng.randrange(-4,5)) for _ in range(3)] for i in range(count)]
 cfg['rejected']=[int(rng.randrange(5)==0) for i in range(count)];cfg['visible']=[rng.choice((0,1,2,256,257)) for i in range(count)];cfg['edges']=[[rng.choice((0,1,2,256,257)) for j in range(count)] for i in range(count)]
 cfg['neighbors']=[];seed_nodes=[]
 for i in range(count):
  adj=list(range(count));rng.shuffle(adj);adj=adj[:rng.randrange(count+1)];cfg['neighbors'].append(adj)
  node=bytearray(rng.randbytes(68));node[:24]=f(*cfg['positions'][i],*cfg['positions'][i]);node[40:52]=w(len(adj),8,ARRAY+0x100+i*0x40);node[53]=cfg['rejected'][i];u.mem_write(nodes[i],bytes(node));u.mem_write(ARRAY+0x100+i*0x40,w(*[nodes[j] for j in adj]));seed_nodes.append(bytes(node))
 u.mem_write(LIST,w(count,8,ARRAY));u.mem_write(ARRAY,w(*nodes[:count]));u.mem_write(Q,bytes(64));u.mem_write(Q,w(nodes[0],0,0,nodes[cfg['goal']]));u.mem_write(Q+28,f(1));u.mem_write(Q+36,w(0x1234));u.mem_write(Q+40,f(cfg['limit'],0));u.mem_write(Q+48,w(0x12345678));u.mem_write(0,w(0xabcdef01))
 query_before=bytes(u.mem_read(Q,64))
 u.mem_write(STACK,w(STOP,Q,ALT if cfg['alternate'] else 0));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,LIST);u.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];path=[];u.emu_start(0x4ce8c0,STOP,count=200000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and r(0)==0xabcdef01
 result=u.reg_read(UC_X86_REG_EAX)&255;assert result in (0,1)
 # Independent ordered open-list model, retaining the original one-time enqueue flag.
 scores=[struct.unpack_from('<f',s,56)[0] for s in seed_nodes];parents=[struct.unpack_from('<I',s,60)[0] for s in seed_nodes];closed=[s[52] for s in seed_nodes];queued=[s[54] for s in seed_nodes]
 for i in range(count):
  if not cfg['rejected'][i]:scores[i]=f32(3.4028234663852886e38);parents[i]=0;closed[i]=0;queued[i]=0
 scores[0]=0.;closed[0]=1;pending=[0];want_trace=[['init'],['push',0]];goal=None
 while pending:
  index=0
  for k in range(1,len(pending)):
   if scores[pending[k]]<scores[pending[index]]:index=k
  i=pending.pop(index);want_trace.append(['pop',i]);found=False
  if cfg['alternate']:
   if i!=0:want_trace.append(['visible',i,cfg['visible'][i]]);found=(cfg['visible'][i]&255)==0
  else:
   delta=[f32(a-b) for a,b in zip(cfg['positions'][i],cfg['positions'][cfg['goal']])];dist=sum(d*d for d in delta)**.5
   if cfg['limit']>0 and dist<cfg['limit']:
    want_trace.append(['visible',i,cfg['visible'][i]]);found=(cfg['visible'][i]&255)==1
   found=found or i==cfg['goal']
  if found:goal=i;break
  for j in cfg['neighbors'][i]:
   if cfg['alternate']:
    want_trace.append(['edge',i,j,cfg['edges'][i][j]])
    if not cfg['edges'][i][j]&255:continue
   if cfg['rejected'][j] or closed[j]:continue
   if not queued[j]:queued[j]=1;pending.append(j);want_trace.append(['push',j])
   delta=[f32(a-b) for a,b in zip(cfg['positions'][j],cfg['positions'][i])];cost=sum(d*d for d in delta)+scores[i]
   if cost<scores[j]:scores[j]=f32(cost);parents[j]=nodes[i]
 want_trace.append(['free']);assert trace==want_trace,('trace',case,trace,want_trace)
 assert result==int(goal is not None)
 for i in range(count):
  want=bytearray(seed_nodes[i]);want[52]=closed[i];want[54]=queued[i];want[60:64]=w(parents[i])
  if not cfg['rejected'][i] or i==0:want[56:60]=f(scores[i])
  assert bytes(u.mem_read(nodes[i],68))==want,('node',case,i)
 expected_path=[];total=0.
 if goal is not None and goal!=0:
  walk=goal
  while walk!=0:expected_path.append(walk);walk=nodes.index(parents[walk]);assert len(expected_path)<=count
  expected_path.append(0);expected_path.reverse()
  for i in expected_path[1:]:total=f32(total+scores[i])
  assert r(Q+48)==struct.unpack('<I',f(total))[0]
 else:assert r(Q+48)==0x12345678
 assert path==expected_path,(case,path,expected_path)
 query_after=bytearray(u.mem_read(Q,64));query_after[48:52]=query_before[48:52];assert query_after==query_before
 successes+=result;alternate+=int(result and cfg['alternate']);near+=int(result and not cfg['alternate'] and goal!=cfg['goal'])
 records.append(dict(case=case,input=cfg,initial_node_words=[list(struct.unpack('<17I',v)) for v in seed_nodes],result=result,path=path,trace=trace,cost_bits=r(Q+48),node_words=[list(struct.unpack('<17I',u.mem_read(nodes[i],68))) for i in range(count)]))
report=dict(result='PASS',cases=len(records),successes=successes,alternate_successes=alternate,near_goal_successes=near,scope='Full original4ce8c0, actual score math, list traversal/removal, SEH restoration and recursive4ceb50 cost/path walk. Scratch-list allocation/append/free, visibility and4cebb0 route-output append supplied. Independent graph model matches exact node state and ordered operations. Not shared code or route storage ownership.',records=records)
(root/'artifacts/ai-search-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})

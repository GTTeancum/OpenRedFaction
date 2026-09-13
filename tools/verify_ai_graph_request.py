"""Original4ce8c0 graph search with real scoring/list removal/path recursion.
Only scratch allocation/append/free, visibility are supplied; route-output append executes unchanged.
"""
import hashlib,json,random,re,struct,sys,subprocess
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
  n=r(this);assert n<8;cpu.mem_write(r(this+8)+4*n,w(arg(0)));cpu.mem_write(this,w(n+1));trace.append(['push',nodes.index(arg(0))]);ret(0,4)
 elif address==0x40ec50:
  assert this==open_owner;trace.append(['free']);ret()
 elif address==0x4cebb0:
  assert arg(0)==Q;path.append(nodes.index(arg(1))) # Actual four-slot append executes.
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x4cebd1);records=[];commands=[];expected=[]
for case in range(2048):
 n=case%7;count=n+2;selections=[rng.choice([0xffffffff,*range(n)]) for _ in range(4)];mode=rng.choice((0,1,2,256,257));search_mode=rng.choice((0,1,2,256,257));limit=rng.choice((0,2,5));seeds=[];neighbors=[]
 for i in range(count):
  adj=list(range(n));rng.shuffle(adj);adj=adj[:rng.randrange(n+1)];neighbors.append(adj)
  node=bytearray(rng.randbytes(68));node[:24]=f(*[rng.randrange(-4,5) for _ in range(6)]);node[28:36]=f(2,2);node[40:52]=w(len(adj),8,ARRAY+0x100+i*0x40);node[64:68]=w(0);u.mem_write(nodes[i],bytes(node));u.mem_write(ARRAY+0x100+i*0x40,w(*[nodes[j] for j in adj],*([0]*(8-len(adj)))));seeds.append(bytes(node))
 u.mem_write(LIST,w(n,8,ARRAY));u.mem_write(ARRAY,w(*nodes[:n],*([0]*(8-n))));u.mem_write(Q,bytes(64));u.mem_write(Q,w(nodes[n+1],*[0 if j==0xffffffff else nodes[j] for j in selections[:2]],nodes[n],*[0 if j==0xffffffff else nodes[j] for j in selections[2:]]));u.mem_write(Q+24,f(.5,1));u.mem_write(Q+32,bytes([mode&255,0,search_mode&255]));u.mem_write(Q+40,f(limit,1));u.mem_write(Q+48,w(0x12345678));u.mem_write(Q+52,w(9,B+0x7000));u.mem_write(B+0x7000,bytes(16));u.mem_write(0,w(0xabcdef01))
 u.mem_write(STACK,w(STOP,Q));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,LIST);u.reg_write(UC_X86_REG_FPCW,0x27f);path=[];trace=[];u.emu_start(0x4cebd0,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP and r(0)==0xabcdef01 and r(LIST)==n
 result=u.reg_read(UC_X86_REG_EAX)&255;route=[nodes.index(r(B+0x7000+j*4)) for j in range(r(Q+52))];after=[];counts=[];tails=[]
 for i in range(count):
  node=bytearray(u.mem_read(nodes[i],68));node[40:52]=seeds[i][40:52];after.append(bytes(node));counts.append(r(nodes[i]+40));tails.append([0 if r(ARRAY+0x100+i*0x40+j*4)==0 else nodes.index(r(ARRAY+0x100+i*0x40+j*4)) for j in range(8)])
 cmd=w(count,n,0)+f(limit)+b''.join(seeds)+bytes((8-count)*68)+w(*[len(v) for v in neighbors],*([0]*(8-count)))+b''.join(w(*v,*([0]*(8-len(v)))) for v in neighbors)+bytes((8-count)*32)+w(*selections,mode,search_mode,0,0)+bytes(256)
 assert len(cmd)==1136;commands.append(cmd)
 want=w(0,result,r(Q+48),len(route),*route,*([0]*(8-len(route))),2166136261)+b''.join(after)+bytes((8-count)*68)+w(*counts,*([0]*(8-count)))+b''.join(w(*v) for v in tails)+bytes((8-count)*32);assert len(want)==884;expected.append(want)
 records.append((n,seeds,neighbors,selections,mode,search_mode,limit))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-graph-request'],input=b''.join(commands))
for i,want in enumerate(expected):assert actual[i*884:(i+1)*884]==want,('PC',i,actual[i*884:i*884+52].hex(),want[:52].hex())
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im);x.mem_map(B,65536)
entry=int(re.search(r'\s_rf_entity_navigation_graph_request_run\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
REFS=B+0x9000;ADJ=B+0xa000;SCRATCH=B+0xb000;XQ=B+0xc000;ROUTE=B+0xd000;OUT=B+0xd100;LISTS=B+0x8000
rx=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
for i,((n,seeds,neighbors,selections,mode,search_mode,limit),want) in enumerate(zip(records,expected)):
 count=n+2
 for j in range(8):
  x.mem_write(nodes[j],seeds[j] if j<count else bytes(68));adj=neighbors[j] if j<count else [];x.mem_write(ADJ+j*32,w(*adj,*([0]*(8-len(adj)))));x.mem_write(REFS+j*16,w(nodes[j],nodes[j],ADJ+j*32,len(adj)));x.mem_write(LISTS+j*12,w(ADJ+j*32,len(adj),8))
 x.mem_write(XQ,w(REFS,LISTS,n)+f(.5,1)+w(mode,search_mode,*selections)+f(limit,1)+w(0x12345678,ROUTE,0,SCRATCH,8));x.mem_write(ROUTE,bytes(20));x.mem_write(OUT,w(99));x.mem_write(STACK,w(STOP,XQ,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 route=[nodes.index(rx(ROUTE+j*4)) for j in range(rx(ROUTE+16))];got=w(x.reg_read(UC_X86_REG_EAX),rx(OUT),rx(XQ+52),len(route),*route,*([0]*(8-len(route))),2166136261)+b''.join(bytes(x.mem_read(a,68)) for a in nodes)+w(*[rx(LISTS+j*12+4) for j in range(count)],*([0]*(8-count)))+bytes(x.mem_read(ADJ,count*32))+bytes((8-count)*32)
 assert got==want,('NXDK',i,got[:52].hex(),want[:52].hex())
# Search validation failure after successful aliased goal insertion must clean both links.
assert n>0
x.mem_write(XQ+24,w(1,0,0xffffffff,0,0));x.mem_write(nodes[n]+12,w(0x7fc00000));x.mem_write(OUT,w(99));before_count=rx(LISTS+4);before_cost=rx(XQ+52)
x.mem_write(STACK,w(STOP,XQ,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0xfffffffe and rx(OUT)==99 and rx(LISTS+4)==before_count and rx(XQ+52)==before_cost
# Insufficient caller scratch is rejected before touching graph or retained output.
x.mem_write(XQ+68,w(0));before=bytes(x.mem_read(B,0xd200));x.mem_write(STACK,w(STOP,XQ,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
assert x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(B,0xd200))==before
report=dict(result='PASS',original_pc_nxdk_cases=2048,compiled_failure_guards=2,successes=sum(struct.unpack_from('<I',v,4)[0] for v in expected),scope='Full original4cebd0 preparation, nearest fallback, endpoint insertion/search/removal and retained output versus concrete shared graph request. Only bounded list growth and search scratch allocation/free supplied. Null-world original visibility shortcut; exact node state, route/cost, adjacency counts and backing slots.0..6 global nodes, optional/aliased endpoint selections and mode bytes. Solid obstruction covered separately; live scene ownership excluded.')
(root/'artifacts/ai-graph-request.json').write_text(json.dumps(report,indent=2));print(report)

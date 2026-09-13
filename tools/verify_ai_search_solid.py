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
  assert this==open_owner;n=r(this);assert n<8;cpu.mem_write(OPEN+4*n,w(arg(0)));cpu.mem_write(this,w(n+1));trace.append(['push',nodes.index(arg(0))]);ret(0,4)
 elif address==0x4ce390:
  assert this==open_owner;trace.append(['pop',nodes.index(r(OPEN+4*arg(0)))]) # Actual stable removal executes.
 elif address==0x40ec50:
  assert this==open_owner;trace.append(['free']);ret()
 elif address==0x4cebb0:
  assert arg(0)==Q;path.append(nodes.index(arg(1))) # Actual four-slot append executes.
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x4ce800);records=[];commands=[];expected=[]
temporary_start='--temporary-start' in sys.argv
WORLD=B+0x8000;FACE=B+0x8200;VERT=B+0x8400;EDGE=B+0x8600
for case in range(2048):
 count=1+case%8;cfg=dict(count=count,goal=rng.randrange(count),alternate=bool(case&1),limit=5.0 if case%3==0 else 0.0)
 cfg['positions']=[[float(rng.randrange(-4,5)) for _ in range(3)] for i in range(count)];cfg['neighbors']=[];seed_nodes=[]
 for i in range(count):
  adj=list(range(count));rng.shuffle(adj);adj=adj[:rng.randrange(count+1)];cfg['neighbors'].append(adj)
  node=bytearray(rng.randbytes(68));node[:24]=f(*cfg['positions'][i],*cfg['positions'][i]);node[40:52]=w(len(adj),8,ARRAY+0x100+i*0x40);node[53]=int(rng.randrange(5)==0);u.mem_write(nodes[i],bytes(node));u.mem_write(ARRAY+0x100+i*0x40,w(*[nodes[j] for j in adj]));seed_nodes.append(bytes(node))
 enabled=int(case%4!=0)
 u.mem_write(WORLD,bytes(256));u.mem_write(WORLD+0x70,w(FACE if enabled else 0));u.mem_write(FACE,bytes(128));u.mem_write(FACE,f(0,0,1,0,-100.0001,-100.0001,-.0001,100.0001,100.0001,.0001));u.mem_write(FACE+0x30,w(-1));u.mem_write(FACE+0x40,w(EDGE));u.mem_write(VERT,f(-100,-100,0,100,-100,0,100,100,0,-100,100,0))
 for j in range(4):u.mem_write(EDGE+j*32,w(VERT+j*12)+bytes(16)+w(EDGE+((j+1)%4)*32,EDGE+((j-1)%4)*32))
 u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,w(15));u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12));u.mem_write(ALT,f(0,0,8))
 u.mem_write(LIST,w(count-int(temporary_start),8,ARRAY));u.mem_write(ARRAY,w(*nodes[int(temporary_start):count]));u.mem_write(Q,bytes(64));u.mem_write(Q,w(nodes[0],0,0,nodes[cfg['goal']]));u.mem_write(Q+28,f(1));u.mem_write(Q+36,w(WORLD));u.mem_write(Q+40,f(cfg['limit'],1));u.mem_write(Q+48,w(0x12345678));u.mem_write(Q+56,w(B+0x7000));u.mem_write(B+0x7000,bytes(16));u.mem_write(0,w(0xabcdef01))
 u.mem_write(STACK,w(STOP,Q,ALT if cfg['alternate'] else 0));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,LIST);u.reg_write(UC_X86_REG_FPCW,0x27f);path=[];trace=[];u.emu_start(0x4ce8c0,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP and r(0)==0xabcdef01
 result=u.reg_read(UC_X86_REG_EAX)&255;retained=path[:4];assert r(Q+52)==len(retained)
 cmd=w(count,cfg['goal'],cfg['alternate'])+f(cfg['limit'])+b''.join(seed_nodes)+bytes((8-count)*68)
 cmd+=w(*[len(v) for v in cfg['neighbors']],*([0]*(8-count)))+b''.join(w(*v,*([0]*(8-len(v)))) for v in cfg['neighbors'])+bytes((8-count)*32)+w(enabled,*([0]*7))+bytes(256)
 assert len(cmd)==1136;commands.append(cmd)
 want=w(0,result,r(Q+48),len(retained),*retained,*([0]*(8-len(retained))),2166136261)+b''.join(bytes(u.mem_read(a,68)) for a in nodes[:count])+bytes((8-count)*68);expected.append(want)
 records.append((cfg.copy(),seed_nodes,enabled))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-search-temporary-start' if temporary_start else '--ai-search-solid'],input=b''.join(commands))
for i,want in enumerate(expected):assert actual[i*596:(i+1)*596]==want,('PC',i,actual[i*596:i*596+52].hex(),want[:52].hex())
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im);x.mem_map(B,65536)
entry=int(re.search(r'\s_rf_entity_navigation_search_solid_members\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
REFS=B+0x9000;ADJ=B+0xa000;SCRATCH=B+0xb000;XQ=B+0xc000;ROUTE=B+0xd000;OUT=B+0xd100
rx=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
for i,((cfg,seeds,enabled),want) in enumerate(zip(records,expected)):
 n=cfg['count']
 for j in range(8):
  x.mem_write(nodes[j],seeds[j] if j<n else bytes(68));adj=cfg['neighbors'][j] if j<n else [];x.mem_write(ADJ+j*32,w(*adj,*([0]*(8-len(adj)))));x.mem_write(REFS+j*16,w(nodes[j],nodes[j],ADJ+j*32,len(adj)))
 x.mem_write(WORLD,bytes(156));x.mem_write(WORLD+148,w(FACE,enabled));x.mem_write(FACE,f(0,0,1,0,-100.0001,-100.0001,-.0001,100.0001,100.0001,.0001)+w(VERT,4,0,0,0,0,0,0));x.mem_write(VERT,f(-100,-100,0,100,-100,0,100,100,0,-100,100,0));x.mem_write(ALT,f(0,0,8))
 x.mem_write(XQ,w(0,cfg['goal'],ALT if cfg['alternate'] else 0)+f(cfg['limit'],1,1)+w(0x12345678));x.mem_write(ROUTE,bytes(20));x.mem_write(OUT,w(99));x.mem_write(STACK,w(STOP,REFS,n,int(temporary_start),n-int(temporary_start),XQ,SCRATCH,8,WORLD,ALT,ROUTE,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 path=[nodes.index(rx(ROUTE+j*4)) for j in range(rx(ROUTE+16))];got=w(x.reg_read(UC_X86_REG_EAX),rx(OUT),rx(XQ+24),len(path),*path,*([0]*(8-len(path))),2166136261)+b''.join(bytes(x.mem_read(a,68)) for a in nodes)
 assert got==want,('NXDK',i,got[:52].hex(),want[:52].hex())
report=dict(result='PASS',original_pc_nxdk_cases=2048,temporary_start=temporary_start,successes=sum(struct.unpack_from('<I',v,4)[0] for v in expected),scope='Complete original search with actual4ce740 visibility,4ce6c0 edges,4cebb0 retained output and flat-world collision. Only original scratch allocation/append/free supplied. Shared PC/NXDK composed search matches exact route/cost/node state. Random graphs, ordinary/alternate mode, nonzero edge threshold, clear/blocked world and near-goal checks. No request endpoint composition or live scene ownership.')
(root/('artifacts/ai-search-temporary-start.json' if temporary_start else 'artifacts/ai-search-solid.json')).write_text(json.dumps(report,indent=2));print(report)

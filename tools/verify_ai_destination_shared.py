"""Shared40ac90 PC/NXDK exact retained-state and boundary trace comparison."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
subprocess.run([sys.executable,str(root/'tools/verify_ai_destination_original.py')],check=True)
records=json.loads((root/'artifacts/ai-destination-original.json').read_text())['records']
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
bits=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
word_offsets=('0x588','0x58c','0x590','0x59c','0x5a0','0x660','0x6bc');vec_offsets=('0x5a4','0x5b0','0x620','0x62c','0x6d4')
codes={'lookup':1,'reset':2,'prepare':3,'limit':4,'nodes':5,'clear_nodes':6,'add_node':7,'direct':8,'search':9,'timer':10}
commands=[];expected=[]
for rec in records:
 c=rec['input'];wire=[rec['initial_words'][o] for o in word_offsets]
 for o in vec_offsets:wire+=rec['initial_vector_bits'][o]
 wire += [c[k] if k!='limit' else bits(c[k]) for k in ('present','action','state','mode','nodes','node_result','limit','direct','search','count','target','coincident')]
 commands.append(w(*wire));out=[0,rec['result']]+[rec['words'][o] for o in word_offsets]
 for o in vec_offsets:out+=rec['vector_bits'][o]
 out+=rec['query'];trace=[]
 for row in rec['trace']:trace.extend([codes[row[0]],*row[1:],*([0]*(3-len(row)))])
 out += [len(trace)//3,*trace,*([0]*(45-len(trace)))];expected.append(w(*out))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-destination'],input=b''.join(commands));assert len(actual)==len(records)*312
for i,want in enumerate(expected):
 got=actual[i*312:(i+1)*312]
 assert got==want,('PC',i,[(j,hex(a),hex(b)) for j,(a,b) in enumerate(zip(struct.unpack('<78I',got),struct.unpack('<78I',want))) if a!=b])
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase
x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im)
B=0x30000000;x.mem_map(B,0x10000);TARGET=B+0x200;POINT=B+0x400;NODES=[B+0x500,B+0x600];Q=B+0x700;BE=B+0x800;OUT=B+0x900;STACK=B+0xe000;STOP=B+0xf000;CB=[STOP+0x100+0x80*i for i in range(9)]
entry=int(re.search(r'\s_rf_entity_ai_destination\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
cfg={};trace=[];calls=0;failure=-1

def hook(cpu,address,size,context):
 global calls
 if address not in CB:return
 calls+=1;sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);status=0
 if failure==calls:status=0xffffffff
 elif address==CB[0]:
  h=arg(1);assert h in (55,77);trace.extend((1,h,0));cpu.mem_write(arg(2),w((B if cfg['present'] else 0) if h==55 else (TARGET if cfg['target'] else 0)))
 elif address==CB[1]:assert arg(1)==B;trace.extend((2,0,0))
 elif address==CB[2]:
  assert arg(1)==B and arg(2)==Q;trace.extend((3,0,0));cpu.mem_write(Q+12,w(bits(1.5),cfg['mode']))
 elif address==CB[3]:
  assert arg(1)==B;trace.extend((4,0,0));cpu.mem_write(arg(2),struct.pack('<d',cfg['limit']))
 elif address==CB[4]:
  assert arg(1)==POINT and arg(2)==bits(1) and arg(3)==bits(2) and arg(4)==Q
  trace.extend((5,cfg['node_result'],0));cpu.mem_write(Q+4,w(NODES[0] if cfg['nodes'] else 0,NODES[1] if cfg['nodes']==2 else 0));cpu.mem_write(arg(5),w(cfg['node_result']))
 elif address==CB[5]:assert arg(1)==B;trace.extend((6,0,0))
 elif address==CB[6]:assert arg(1)==B;trace.extend((7,NODES.index(arg(2)),0))
 elif address==CB[7]:
  assert arg(1)==B and arg(2)==(TARGET if cfg['action']==3 and cfg['target'] else 0);trace.extend((8,cfg['direct'],0));cpu.mem_write(arg(3),w(cfg['direct']))
 else:
  assert arg(1)==Q;trace.extend((9,cfg['search'],cfg['count']));cpu.mem_write(Q+28,w(cfg['count']));cpu.mem_write(arg(2),w(cfg['search']))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
def setup(rec):
 global cfg,trace,calls
 cfg=rec['input'];trace=[];calls=0;x.mem_write(B,bytes(136));x.mem_write(B,w(55,cfg['action'],cfg['state'],77,bits(1),bits(2),bits(2),bits(3),bits(4),bits(6),bits(7),bits(8)))
 for off,key in zip((72,84,48,60,96),vec_offsets):x.mem_write(B+off,w(*rec['initial_vector_bits'][key]))
 x.mem_write(B+108,w(*[rec['initial_words'][o] for o in word_offsets]))
 x.mem_write(POINT,w(bits(4),bits(5),bits(6)))
 for i,node in enumerate(NODES):
  x.mem_write(node,bytes(68));x.mem_write(node+4,w(bits(10)));x.mem_write(node+12,w(bits(8 if i and not cfg['coincident'] else 0),bits(12 if i and not cfg['coincident'] else 8),0));x.mem_write(node+28,w(bits(4),bits(2)))
 x.mem_write(Q,bytes(32));x.mem_write(BE,w(*CB,0));x.mem_write(OUT,w(123));x.mem_write(STACK,w(STOP,55,POINT,12345,Q,BE,OUT));x.reg_write(UC_X86_REG_ESP,STACK)
def run():
 x.emu_start(entry,STOP,count=200000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
for i,rec in enumerate(records):
 setup(rec);status=run();result=r(OUT)
 if not status and result:trace.extend((10,0,0))
 fields=list(struct.unpack('<7I',x.mem_read(B+108,28)))
 if fields[1]==B+72:fields[1]=0x300005a4
 if fields[2]==B+48:fields[2]=0x30000620
 out=[status,result,*fields]
 for off in (72,84,48,60,96):out.extend(struct.unpack('<3I',x.mem_read(B+off,12)))
 query=list(struct.unpack('<8I',x.mem_read(Q,32)));query[0]=int(query[0]!=0);query[1]=int(query[1]!=0);query[2]=2*int(query[2]!=0)
 out += query+[len(trace)//3,*trace,*([0]*(45-len(trace)))];want=expected[i]
 assert w(*out)==want,('NXDK',i,[(j,hex(a),hex(b)) for j,(a,b) in enumerate(zip(out,struct.unpack('<78I',want))) if a!=b])
rec=next(r for r in records if r['input']['nodes']==2 and r['input']['action']==3 and any(t[0]=='search' for t in r['trace']))
setup(rec);run();boundary_count=calls
for failure in range(1,boundary_count+1):
 setup(rec);assert run()==0xffffffff and calls==failure and r(OUT)==123
 if failure<=2:assert r(B+108)==rec['initial_words']['0x588'] and bytes(x.mem_read(B+48,12))==w(*rec['initial_vector_bits']['0x620'])
 else:assert bytes(x.mem_read(B+48,12))==w(bits(4),bits(5),bits(6))
failure=-1
# Invalid clocks reject before lookup. Degenerate horizontal normalization retains prior destination writes.
for bad in (-1,0x7fffffff):
 setup(rec);x.mem_write(STACK+12,w(bad));before=bytes(x.mem_read(B,136));assert run()==0xfffffffc and not calls and r(OUT)==123 and bytes(x.mem_read(B,136))==before
rec=next(r for r in records if r['input']['present'] and r['input']['mode']==1 and r['input']['nodes'] and (r['input']['node_result']&255)!=1 and not (r['input']['action']==3 and r['input']['state']==3))
setup(rec);x.mem_write(NODES[0]+12,w(bits(4),bits(8),bits(6)))
status=run();assert status==0xfffffffe and r(OUT)==123 and bytes(x.mem_read(B+48,12))==w(bits(4),bits(5),bits(6))
assert not any(trace[i]==6 for i in range(0,len(trace),3))
report=dict(result='PASS',original_pc_nxdk_cases=len(records),callback_failures=boundary_count,domain_guards=3,scope='Full shared40ac90 exact actor/query state and ordered external trace versus original PC executable, including geometry and partial failure. PC and compiled NXDK checked. Navigation callbacks remain supplied; no live routing or native XEMU claim.')
(root/'artifacts/ai-destination-shared.json').write_text(json.dumps(report,indent=2));print(report)

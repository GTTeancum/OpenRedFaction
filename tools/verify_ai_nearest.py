"""Full4ce570 nearest-node selection versus PC/NXDK, exact cutoff and flags."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;NODES=[B+i*0x100 for i in range(8)];Q=B+0x1000;POINT=B+0x2000;LIST=B+0x3000;REFS=B+0x4000;BE=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xf000;CB=[STOP+0x100,STOP+0x200]
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,0x10000);return m
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_nearest\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
visible=[];edges=[];trace=[];failure=-1;calls=0

def hook(m,address,size,context):
 global calls
 original=m is u;ops=(0x4ce6c0,0x4ce740) if original else CB
 if address not in ops:return
 calls+=1;sp=m.reg_read(UC_X86_REG_ESP);arg=lambda i:r(m,sp+4+4*i);base=0 if original else 1;op=ops.index(address)+1
 if op==1:
  assert arg(base)==77 and arg(base+1)==POINT and arg(base+3)==0x3f800000;i=NODES.index(arg(base+2)-12);value=edges[i]
 else:
  if original:assert arg(0)==0x1234
  i=NODES.index(arg(base if not original else 1));assert arg(base+1 if not original else 2)==POINT
  assert arg(base+2 if not original else 3)==0x3f000000 and arg(base+3 if not original else 4)==0x3f800000;value=visible[i]
 status=0
 if calls==failure and not original:status=0xffffffff
 else:
  trace.append([op,i,value])
  if not original:m.mem_write(arg(5),w(value))
 m.reg_write(UC_X86_REG_EAX,value if original else status);m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
def call(m,entry,args,pop=0):
 m.mem_write(STACK,w(STOP,*args));m.reg_write(UC_X86_REG_ESP,STACK);m.reg_write(UC_X86_REG_ECX,LIST);m.reg_write(UC_X86_REG_FPCW,0x27f);m.emu_start(entry,STOP,count=100000);assert m.reg_read(UC_X86_REG_EIP)==STOP and m.reg_read(UC_X86_REG_ESP)==STACK+4+pop;return m.reg_read(UC_X86_REG_EAX)
def hash_trace():
 h=2166136261
 for row in trace:
  for v in row:h=((h^v)*16777619)&0xffffffff
 return h
rng=random.Random(0x4ce570);commands=[];expected=[];hits=0;predicate_calls=0;cutoffs=0
for case in range(2048):
 count=case%9;alternate=77 if case&1 else 0;point=[rng.uniform(-5,5) for _ in range(3)];nodes=[];visible=[rng.choice((0,1,2,256,257)) for i in range(8)];edges=[rng.choice((0,1,2,256,257)) for i in range(8)]
 if case%8==0:count=2;point=[0,0,0];visible=[1]*8;edges=[1]*8
 for i in range(8):
  n=bytearray(rng.randbytes(68));n[12:24]=f(*[rng.uniform(-55,55) for _ in range(3)]);n[53]=rng.choice((0,1,2,255))
  if case%8==0:n[12:24]=f(50,.001 if i==1 else 0,0);n[53]=0
  nodes.append(bytes(n))
 command=w(count,alternate)+f(*point,.5,1,1)+b''.join(nodes)+w(*visible,*edges);commands.append(command)
 for m in (u,x):
  for a,n in zip(NODES,nodes):m.mem_write(a,n)
  m.mem_write(POINT,f(*point))
 u.mem_write(Q,bytes(64));u.mem_write(Q+24,f(.5,1));u.mem_write(Q+36,w(0x1234));u.mem_write(Q+44,f(1));u.mem_write(LIST,w(count,8,REFS));u.mem_write(REFS,w(*NODES));trace=[];calls=0;token=call(u,0x4ce570,[Q,POINT,alternate],12);hits+=bool(token);predicate_calls+=calls
 after=b''.join(bytes(u.mem_read(a,68)) for a in NODES);output=w(0,token,hash_trace())+after;expected.append(output)
 if case%8==0:assert token==B and u.mem_read(B+0x134,1)==b'\x01' and r(u,B+0x138)==struct.unpack('<I',f(2500))[0];cutoffs+=1
 x.mem_write(REFS,b''.join(w(a,a,0,0) for a in NODES));x.mem_write(BE,w(*CB,0));x.mem_write(OUT,w(99));trace=[];calls=0
 status=call(x,entry,[REFS,count,POINT,0x3f000000,0x3f800000,alternate,0x3f800000,BE,OUT]);actual=w(status,r(x,OUT),hash_trace())+b''.join(bytes(x.mem_read(a,68)) for a in NODES);assert actual==output,('NXDK',case,actual[:12].hex(),output[:12].hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-nearest'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
# Fail each stage with two reachable nodes; reached scoring persists, output token does not change.
for failure in (1,2):
 visible=[1]*8;edges=[1]*8;x.mem_write(POINT,f(0,0,0));x.mem_write(NODES[0],bytes(68));x.mem_write(NODES[0]+12,f(1,0,0));x.mem_write(OUT,w(99));calls=0
 assert call(x,entry,[REFS,1,POINT,0x3f000000,0x3f800000,77,0x3f800000,BE,OUT])==0xffffffff and calls==failure and r(x,OUT)==99 and r(x,NODES[0]+56)==0x3f800000
report=dict(result='PASS',original_pc_nxdk_cases=2048,selected=hits,predicate_calls=predicate_calls,cutoff_rounding_cases=cutoffs,callback_failures=2,scope='Full original4ce570 with actual list/distance; edge/visibility predicates supplied. Exact shared PC/NXDK selection, callback order and node bytes; strict2500 cutoff before float score store, rejected byte exactly1 and low-byte predicate rules. No actual scene visibility or wrapper binding.')
(root/'artifacts/ai-nearest.json').write_text(json.dumps(report,indent=2));print(report)

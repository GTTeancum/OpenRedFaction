"""Unhooked full40b0d0 direct route against shared PC/NXDK geometry and ordering."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;A=B+0x2000;T=B+0x4000;WORLD=B+0x5000;REFS=B+0x5800;START=B+0x6000;END=B+0x6100;MOV=B+0x6200;OUT=B+0x6300;STACK=B+0xe000;STOP=B+0xf000
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,0x10000);return m
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_ai_direct_route\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(m,entry,args):
 m.mem_write(STACK,w(STOP,*args));m.reg_write(UC_X86_REG_ESP,STACK);m.reg_write(UC_X86_REG_FPCW,0x27f);m.emu_start(entry,STOP,count=200000);assert m.reg_read(UC_X86_REG_EIP)==STOP;return m.reg_read(UC_X86_REG_EAX)
u.mem_write(0x1754474,b'\x07');u.mem_write(0x6460e8,w(WORLD));rng=random.Random(0x40b0d0);commands=[];expected=[];outcomes=[0,0,0,0]
for case in range(2048):
 count=case%5;state=rng.choice((1,2,3));kind=rng.randrange(20);target=case&1;order=list(range(4));rng.shuffle(order);addresses=[B+0x1000+j*0x80 for j in order]
 start=[rng.uniform(-10,10) for _ in range(3)];end=[rng.uniform(-10,10) for _ in range(3)];pos=[rng.uniform(-10,10) for _ in range(3)];radius,height,tr,th=[rng.uniform(.1,2) for _ in range(4)];nodes=[];neighbors=[]
 if case%8==0:count=4;state=1;kind=0;start=[0,0,0];end=[1,0,0];pos=[0,0,0];radius=.5;height=1;tr=.5;th=1
 if case%8==1:count=1;state=1;kind=case%20;start=[0,0,0];end=[1,0,0];pos=[0,0,0];radius=.5;height=1;tr=.5;th=1
 for i in range(4):
  n=bytearray(rng.randbytes(68));n[:12]=f(*[rng.uniform(-8,8) for _ in range(3)]);n[28:36]=f(rng.uniform(.1,6),rng.uniform(.1,8))
  if case%8==0:n[:12]=f((-6,-3,3,6)[i],0,0);n[28:36]=f(1,4)
  if case%8==1:n[:12]=f(0,0,0);n[28:36]=f(3,4)
  choices=list(range(count));rng.shuffle(choices);adj=choices if case%8==0 else choices[:rng.randrange(count+1)];neighbors.append(adj);n[40:52]=w(len(adj),4,B+0x7000+i*0x40);nodes.append(bytes(n))
  u.mem_write(B+0x7000+i*0x40,w(*[addresses[j] for j in adj]));x.mem_write(B+0x8000+i*0x40,w(*adj))
 wire=w(count,state,kind,target)+f(*start,*end,*pos,radius,height,tr,th)+w(*order)+b''.join(nodes)+w(*[len(v) for v in neighbors])+b''.join(w(*(v+[0]*(4-len(v)))) for v in neighbors)
 seed=bytearray(rng.randbytes(0x1500));seed[0x554:0x558]=w(state);seed[0x858:0x85c]=w(MOV);seed[0x7c0:0x7c8]=f(radius,height);seed[0x3c:0x48]=f(*pos);seed[0x69c:0x6a4]=w(11,22);u.mem_write(A,bytes(seed));u.mem_write(T+0x7c0,f(tr,th));u.mem_write(MOV+4,w(kind));u.mem_write(WORLD+0x300,w(count,4,REFS));u.mem_write(REFS,w(*addresses))
 for m in (u,x):
  m.mem_write(START,f(*start));m.mem_write(END,f(*end))
  for a,n in zip(addresses,nodes):m.mem_write(a,n)
 result=call(u,0x40b0d0,[START,A,END,T if target else 0])&255;assert result in (0,1)
 changed=bytearray(u.mem_read(A,len(seed)));changed[0x69c:0x6a4]=seed[0x69c:0x6a4];assert changed==seed
 tokens=[r(u,A+0x69c),r(u,A+0x6a0)];outcomes[0 if not result else 1 if state==3 else 2 if tokens[1]==22 else 3]+=1
 after=b''.join(bytes(u.mem_read(a,68)) for a in addresses);output=w(0,result,*tokens)+after
 x.mem_write(A,bytes(152));x.mem_write(A+8,w(state));x.mem_write(A+16,f(radius,height));x.mem_write(A+24,f(*pos));x.mem_write(A+136,w(kind,0,11,22));x.mem_write(T,bytes(152));x.mem_write(T+16,f(tr,th));x.mem_write(OUT,w(99));x.mem_write(REFS,b''.join(w(a,a,B+0x8000+i*0x40,len(neighbors[i])) for i,a in enumerate(addresses)))
 status=call(x,entry,[A,T if target else 0,START,END,REFS,count,OUT]);got=w(status,r(x,OUT),r(x,A+144),r(x,A+148))+b''.join(bytes(x.mem_read(a,68)) for a in addresses)
 assert got==output,('NXDK',case,got[:16].hex(),output[:16].hex())
 commands.append(wire);expected.append(output)
assert all(outcomes),outcomes
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-direct'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
# Malformed adjacency rejects before candidate writes, except state3 never reaches the collection.
x.mem_write(A+8,w(1));x.mem_write(OUT,w(99));x.mem_write(REFS,w(addresses[0],addresses[0],B+0x8000,1));x.mem_write(B+0x8000,w(1));before=bytes(x.mem_read(addresses[0],68));assert call(x,entry,[A,0,START,END,REFS,1,OUT])==0xfffffffc and r(x,OUT)==99 and bytes(x.mem_read(addresses[0],68))==before
x.mem_write(A+8,w(3));assert call(x,entry,[A,0,START,END,0,1,OUT])==0 and r(x,OUT)==1
report=dict(result='PASS',original_pc_nxdk_cases=2048,rejected_shortcut_single_pair=outcomes,compiled_guards=2,scope='Unhooked original40b0d0 including movement predicates, global node list and actual single/pair geometry/distance. Exact PC/NXDK route tokens, result and all mutable node bytes. Random finite geometry, permuted pointer order, ordered adjacency, target sizes, ties and state3 shortcut. No live scene or searched route claim.')
(root/'artifacts/ai-direct.json').write_text(json.dumps(report,indent=2));print(report)

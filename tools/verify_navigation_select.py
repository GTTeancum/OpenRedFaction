"""Full40c2c0 ordered navigation selection versus shared PC/compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;node=base+0x100;out=base+0x200;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_select\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(address,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
level=base+0x4000;refs=base+0x2000;selection=base+0x3000;callback=base+0xf100
word=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
u.mem_write(0x1754474,b'\x07');u.mem_write(0x6460e8,w(level))
addresses=[];trace=[];mask=0

def hook(cpu,address,size,data):
 if address not in (0x4991c0,callback):return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x4991c0:
  args=struct.unpack('<7I',cpu.mem_read(sp+4,28));assert args[1:]==(base,0x40200000,1,0,0,0),args
  start=args[0]
 else:
  args=struct.unpack('<5I',cpu.mem_read(sp+4,20));assert args[0]==0 and args[2:4]==(base,0x40200000),args
  start=args[1]
 index=addresses.index(start-12);trace.append(index);blocked=(mask>>index)&1
 if address==callback:cpu.mem_write(args[4],w(blocked));result=0
 else:result=blocked
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x40c2c0);commands=[];expected=[];counts=[0,0,0,0];visibility_calls=0
for case in range(2048):
 count=case%5;mode=rng.choice((0,1,2,256,257));far=rng.choice((0,1,256));mask=rng.randrange(16);order=list(range(4));rng.shuffle(order)
 addresses=[base+0x1000+j*0x80 for j in order];point=[rng.uniform(-12,12) for _ in range(3)];radius=rng.uniform(.1,2);height=rng.uniform(.1,3);nodes=[]
 for i in range(4):
  n=bytearray(rng.randbytes(68));n[:12]=f(*[rng.uniform(-8,8) for _ in range(3)]);n[28:36]=f(rng.uniform(.1,6),rng.uniform(.1,8));n[64:68]=w(rng.randrange(2));nodes.append(bytes(n))
 if case%8==0:
  count=4;point=[0,0,(case//8)%3];radius=.5;height=1
  for i in range(4):
   n=bytearray(nodes[i]);n[:12]=f((-6,-3,3,6)[i],0,0);n[28:36]=f(1,4);n[64:68]=w(0);nodes[i]=bytes(n)
 neighbors=[]
 for i in range(4):
  choices=list(range(count));rng.shuffle(choices);neighbors.append(choices if case%8==0 else choices[:rng.randrange(count+1)])
  n=bytearray(nodes[i]);n[40:52]=w(len(neighbors[i]),4,base+0x6000+i*0x40);nodes[i]=bytes(n)
  u.mem_write(base+0x6000+i*0x40,w(*[addresses[j] for j in neighbors[i]]))
  x.mem_write(base+0x7000+i*0x40,w(*neighbors[i]))
 wire=w(count,mode,far,mask)+f(*point,radius,height)+w(*order)+b''.join(nodes)+w(*[len(n) for n in neighbors])+b''.join(w(*(n+[0]*(4-len(n)))) for n in neighbors)
 for cpu in (u,x):
  cpu.mem_write(base,f(*point));cpu.mem_write(selection,w(99,99,99))
  for addr,n in zip(addresses,nodes):cpu.mem_write(addr,n)
 u.mem_write(level+0x300,w(count,4,refs));u.mem_write(refs,w(*addresses));trace=[]
 result=call(u,0x40c2c0,[base,struct.unpack('<I',f(radius))[0],struct.unpack('<I',f(height))[0],mode,selection,selection+4,far])&255
 indices=[0xffffffff if word(u,selection+i*4)==0 else addresses.index(word(u,selection+i*4)) for i in range(2)]
 assert result in (0,1);after=b''.join(bytes(u.mem_read(a,68)) for a in addresses);original_trace=trace[:];visibility_calls+=len(trace)
 counts[0 if result and indices[1]==0xffffffff else 1 if result else 2 if indices[0]!=0xffffffff else 3]+=1
 output=w(0,*indices,result,len(trace),*(trace+[0]*(4-len(trace))))+after;assert len(output)==308
 x.mem_write(refs,b''.join(w(a,k,base+0x7000+i*0x40,len(neighbors[i])) for i,(a,k) in enumerate(zip(addresses,order))));trace=[]
 status=call(x,entry,[refs,count,base,struct.unpack('<I',f(radius))[0],struct.unpack('<I',f(height))[0],mode,far,callback,0,selection])
 assert status==0,(case,'status',status)
 assert bytes(x.mem_read(selection,12))==w(*indices,result),(case,'selection',indices,result,bytes(x.mem_read(selection,12)).hex())
 assert trace==original_trace and b''.join(bytes(x.mem_read(a,68)) for a in addresses)==after,(case,'trace/nodes',trace,original_trace)
 commands.append(wire);expected.append(output)
# Malformed adjacency must fail before resetting selections or candidate flags.
for bad in range(3):
 x.mem_write(selection,w(99,99,99));x.mem_write(base+0x7000,w(1));x.mem_write(refs,w(0 if bad==0 else addresses[0],0,base+0x7000,1 if bad==1 else 0))
 before=bytes(x.mem_read(addresses[0],68));trace=[]
 status=call(x,entry,[refs,1,base,0x3f000000,0x3f800000,0,0,0 if bad==2 else callback,0,selection])
 assert status!=0 and bytes(x.mem_read(selection,12))==w(99,99,99) and bytes(x.mem_read(addresses[0],68))==before and not trace
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-select'],input=b''.join(commands));assert actual==b''.join(expected),'PC mismatch'
report=dict(result='PASS',cases=2048,single_pair_fallback_none=counts,visibility_calls=visibility_calls,preflight_guards=3,scope='Complete original40c2c0 with all candidate/geometry helpers unchanged; only4991c0 visibility supplied. Exact PC/compiled NXDK selection, return meaning, ordered visibility calls and full mutable node bytes.0..4 nodes, permuted address order and per-node neighbor lists, finite randomized geometry, low-byte mode/far flags and blocked fallback candidates. Stable collection; callback does not mutate nodes. Visibility implementation, resource loading, live AI and native XEMU excluded.')
(root/'artifacts/navigation-select-verification.json').write_text(json.dumps(report,indent=2));print(report)

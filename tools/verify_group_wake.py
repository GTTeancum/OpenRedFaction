"""Original controller activation wake loops, with real list and flag helpers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP
base=0x30000000;stack=base+0xc000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(im)+4095)//4096*4096);m.mem_write(b,im);m.mem_map(base,65536);return m
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_group_wake_objects\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x46ae65);commands=bytearray();expected=bytearray();changed=[0,0]
for n in range(2048):
 count=n%35;nodes=[];bounds=[];attached=[rng.randrange(95,112) for _ in range(4)];parents=[rng.randrange(195,212) for _ in range(4)]
 for j in range(34):
  lo=[rng.randrange(-4,5) for _ in range(3)];hi=[v+rng.randrange(0,5) for v in lo]
  if n%17==0:lo=[100]*3 if j<32 else [-10]*3;hi=[101]*3 if j<32 else [10]*3
  bounds.append(struct.pack('<6f',*lo,*hi))
 for i in range(8):
  lo=[rng.randrange(-4,5) for _ in range(3)];hi=[v+rng.randrange(0,5) for v in lo]
  flags=rng.choice([0,0x4000,8,0x6000000]);physics=rng.getrandbits(31);cls=rng.choice([0,0x400,0x800])
  nodes.append(w(100+i,rng.randrange(195,212),flags,physics,cls,int(i>=4))+struct.pack('<6f',*lo,*hi))
 wire=w(count)+b''.join(nodes)+b''.join(bounds)+w(*attached,*parents);commands.extend(wire)
 u.mem_write(base,bytes(4096));u.mem_write(base+0x2c0,w(4,4,base+0x400));u.mem_write(base+0x400,w(*attached))
 u.mem_write(0x5cb2ec,w(base+0x1000));u.mem_write(0x8723b4,w(base+0x5000));u.mem_write(0x64e63c,w(base+0xa000))
 for i,p in enumerate(parents):
  node=base+0xa000+i*0x300;u.mem_write(node,bytes(0x300));u.mem_write(node+0x2c,w(p));u.mem_write(node+0x28c,w(node+0x300 if i<3 else 0x64e3b0))
 before=[]
 for i,node in enumerate(nodes):
  ptr=base+0x1000+i*0x1000;handle,parent,flags,physics,cls,family=struct.unpack('<6I',node[:24]);data=bytearray([0xa5]*4096)
  for offset,value in ((0x2c,handle),(0x30,parent),(0x7c,flags),(0x1a8,physics),(0x294,ptr+0x800),(0xf24,cls)):
   data[offset:offset+4]=w(value)
  data[0x190:0x1a8]=node[24:];data[0x28c:0x290]=w(ptr+0x1000 if i not in (3,7) else (0x5cb060 if i==3 else 0x872128))
  before.append(data);u.mem_write(ptr,bytes(data))
 u.mem_write(stack+0x14,w(min(count,32)));u.mem_write(stack+0x34,b''.join(bounds[:32]));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base);u.reg_write(UC_X86_REG_EBP,min(count,32))
 u.emu_start(0x46ae65,0x46af8d,count=200000);assert u.reg_read(UC_X86_REG_EIP)==0x46af8d
 result=[]
 for i,node in enumerate(nodes):
  ptr=base+0x1000+i*0x1000;after=bytes(u.mem_read(ptr,4096));data=before[i]
  for offset in (0x7c,0x1a8):data[offset:offset+4]=after[offset:offset+4]
  assert bytes(data)==after,(n,i,'unexpected object mutation')
  value=node[:8]+after[0x7c:0x80]+after[0x1a8:0x1ac]+node[16:];result.append(value);changed[int(i>=4)]+=value!=node
 want=w(0)+b''.join(result);expected.extend(want)
 x.mem_write(base,wire);x.mem_write(stack,w(stop,base+4,8,base+388,count,base+1204,4,base+1220,4));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=200000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+4,384));assert got==want,('NXDK',n)
# Invalid family must reject before any object changes.
guard=bytearray(wire);guard[4+7*48+20:4+7*48+24]=w(2)
want=w(-4)+bytes(guard[4:388]);commands.extend(guard);expected.extend(want)
x.mem_write(base,bytes(guard));x.mem_write(stack,w(stop,base+4,8,base+388,count,base+1204,4,base+1220,4));x.reg_write(UC_X86_REG_ESP,stack)
x.emu_start(entry,stop,count=200000);assert x.reg_read(UC_X86_REG_EIP)==stop
assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+4,384))==want
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-wake'],input=commands);assert actual==expected
assert all(changed)
report=dict(result='PASS',cases=2048,port_guards=1,entity_changes=changed[0],second_list_changes=changed[1],original_sha256=digest,scope='Original 46ae65..46af8d loops and all list-search, strict overlap, class and wake helpers execute unchanged without hooks. Prepared ordered bounds, actual circular lists and controller exclusion array; first32 bounds only, touching and zero-volume cases. Full object mutation checked, exact PC/NXDK flags. Mover-handle bounds gathering and live world snapshot ownership excluded.')
(root/'artifacts/group-wake-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

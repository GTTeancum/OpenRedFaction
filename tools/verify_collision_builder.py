"""Compare complete original collision-tree construction with PC and NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000

allocated=[]
def allocate(uc,address,size,data):
 pointer=base+len(allocated)*64;allocated.append(pointer);assert len(allocated)<=31
 uc.mem_write(pointer,bytes(64));sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0]
 uc.reg_write(UC_X86_REG_EAX,pointer);uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,allocate,begin=0x4f97b0,end=0x4f97b0)
rng=random.Random(0x4f9340);cases=[];trees=[];max_nodes=0
for n in range(600):
 count=rng.randrange(1,17);boxes=[]
 for i in range(16):
  center=[rng.randrange(-20,21) for _ in range(3)];width=[rng.choice([0,.25,1,5,20]) for _ in range(3)]
  boxes.append([center[j]-width[j] for j in range(3)]+[center[j]+width[j] for j in range(3)])
 wire=struct.pack('<96fII',*[v for box in boxes for v in box],count,100000);cases.append(wire)
 allocated[:]=[base];u.mem_write(base,bytes(64));faces=base+8192
 for i in range(count):
  face=faces+i*128;u.mem_write(face,bytes(128));u.mem_write(face+0x10,wire[i*24:(i+1)*24]);u.mem_write(face+0x58,struct.pack('<I',faces+(i+1)*128 if i+1<count else 0));u.mem_write(face+0x48,struct.pack('<I',base))
 u.mem_write(base+0x18,struct.pack('<II',faces,count))
 for entry,args in [(0x4f8fd0,[]),(0x4f9050,[0])]:
  u.mem_write(stack,struct.pack('<'+str(len(args)+1)+'I',stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
  u.emu_start(entry,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 tree=[]
 for pointer in allocated:
  bounds=bytes(u.mem_read(pointer,24));first,num,left,right=struct.unpack('<4I',u.mem_read(pointer+24,16));order=[]
  while first:
   order.append((first-faces)//128);first=struct.unpack('<I',u.mem_read(first+0x58,4))[0];assert len(order)<=count
  assert len(order)==num
  tree.append((bounds,order,allocated.index(left) if left else 0xffffffff,allocated.index(right) if right else 0xffffffff))
 trees.append(tree);max_nodes=max(max_nodes,len(tree))
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--build'],input=b''.join(cases));at=0
for n,tree in enumerate(trees):
 status,num,peak=struct.unpack_from('<iII',raw,at);at+=12;assert status==0 and num==len(tree),(n,status,num,len(tree))
 nodes=raw[at:at+num*40];at+=num*40;count=struct.unpack_from('<I',cases[n],384)[0];ids=struct.unpack_from('<'+str(count)+'I',raw,at);at+=count*4
 assert sorted(ids)==list(range(count))
 for i,(bounds,order,left,right) in enumerate(tree):
  node=nodes[i*40:(i+1)*40];first,size,l,r=struct.unpack_from('<4I',node,24)
  assert node[:24]==bounds and list(ids[first:first+size])==order and (l,r)==(left,right),(n,i,node.hex(),bounds.hex(),ids[first:first+size],order,(l,r),(left,right))
assert at==len(raw)
# Exact budget boundary, empty input and malformed bounds are port contracts.
guards=[];expected=[]
for count,budget,status in [(16,3852,0),(16,3851,-4),(0,40,0),(0,39,-4)]:
 guards.append(cases[0][:384]+struct.pack('<II',count,budget));expected.append(status)
for offset,value in [(0,math.nan),(0,10000)]:
 wire=bytearray(cases[0]);struct.pack_into('<f',wire,offset,value);guards.append(bytes(wire));expected.append(-2)
guard_raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--build'],input=b''.join(guards));at=0
for wire,want in zip(guards,expected):
 status,num,peak=struct.unpack_from('<iII',guard_raw,at);assert status==want,(status,want)
 at+=12+(num*40+struct.unpack_from('<I',wire,384)[0]*4 if status==0 else 0)
assert at==len(guard_raw)
cases+=guards;raw+=guard_raw
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
symbols=(root/'build/xbox/main.map').read_text()
def symbol(name):
 match=re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',symbols);assert match,name
 return int(match.group(1),16)
heap=[base+16384];live={};fail=[0];attempt=[0]
def allocator(uc,address,size,data):
 sp=uc.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<II',uc.mem_read(sp,8))
 if address==symbol('malloc'):
  attempt[0]+=1
  if attempt[0]==fail[0]:pointer=0
  else:
   pointer=heap[0];heap[0]+=(arg+15)&~15;assert heap[0]<base+48000;live[pointer]=arg
  uc.reg_write(UC_X86_REG_EAX,pointer)
 elif arg:assert arg in live;del live[arg]
 uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
for name in ('malloc','free'):x.hook_add(UC_HOOK_CODE,allocator,begin=symbol(name),end=symbol(name))
def call(name,args):
 x.mem_write(stack,struct.pack('<'+str(len(args)+1)+'I',stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(symbol(name),stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 return x.reg_read(UC_X86_REG_EAX)
at=0
for n,wire in enumerate(cases):
 count,budget=struct.unpack_from('<II',wire,384);faces=base;out=base+4096;heap[0]=base+16384;attempt[0]=0
 for i in range(16):x.mem_write(faces+i*72,bytes(16)+wire[i*24:(i+1)*24]+bytes(32))
 x.mem_write(out,bytes([0xa5])*40);status=call('rf_collision_tree_open',[faces,count,budget,out])
 if status:
  assert bytes(x.mem_read(out,40))==bytes([0xa5])*40
  got=struct.pack('<III',status,0,0)
 else:
  storage,nodes,views,ids,work,num,total,cap,retained,peak=struct.unpack('<10I',x.mem_read(out,40))
  got=struct.pack('<III',0,num,peak)+(bytes(x.mem_read(nodes,num*40)) if num else b'')+(bytes(x.mem_read(ids,count*4)) if count else b'')
  call('rf_collision_tree_close',[out]);assert bytes(x.mem_read(out,40))==bytes(40)
 assert got==raw[at:at+len(got)],('NXDK',n);at+=len(got);assert not live
assert at==len(raw)
for failure in (1,2):
 heap[0]=base+16384;attempt[0]=0;fail[0]=failure;x.mem_write(out,bytes([0xa5])*40)
 # Use a known valid face to reach both allocation boundaries.
 x.mem_write(faces,bytes(72))
 assert call('rf_collision_tree_open',[faces,1,100000,out])==0xffffffff
 assert bytes(x.mem_read(out,40))==bytes([0xa5])*40 and not live
report=dict(result='PASS',original_cases=len(trees),port_guards=len(guards),nxdk_allocation_failure_guards=2,max_nodes=max_nodes,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Complete original union-bounds and recursive partition builder; only pool allocation 4f97b0 replaced with bounded node storage. All original list movement, partition decisions and child initialization run unchanged. Exact node bounds, ordered face identities, child topology versus PC and NXDK. NXDK malloc/free replaced with bounded test storage; this is a CPU fixture, not an XEMU allocation test.')
(root/'artifacts/collision-builder-verification.json').write_text(json.dumps(report,indent=2));print(report)

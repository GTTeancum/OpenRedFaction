"""Execute original4cc010 to verify nested resource recycling and retained payload."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
binary=root/'Installed_Game/RF.exe';digest=hashlib.sha256(binary.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(binary));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;u.mem_map(b,0x20000);stack=b+0x1e000;stop=b+0x1f000
w=lambda v:struct.pack('<I',v&0xffffffff)
put=lambda a,v:u.mem_write(a,w(v))
get=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def link(head,nodes,offset):
 seq=[head]+nodes
 for i,node in enumerate(seq):put(node+offset,seq[(i+1)%len(seq)]);put(node+offset+4,seq[i-1])
def walk(head,offset):
 result=[];node=get(head+offset);previous=head
 while node!=head:
  assert len(result)<100 and get(node+offset+4)==previous
  result.append(node);previous=node;node=get(node+offset)
 assert get(head+offset+4)==previous
 return result
rng=random.Random(0x4cc010);total_middle=total_leaf=0
for case in range(256):
 u.mem_write(b,rng.randbytes(0x10000))
 root_node=b;other=b+0x100;mid=[b+0x1000+j*0x100 for j in range(case%7)]
 leaves=[];cursor=b+0x4000
 link(0x878e18,[other,root_node],0x28);link(root_node+4,mid,0x1c)
 for node in mid:
  group=[cursor+j*0x40 for j in range(rng.randrange(6))];cursor+=0x200
  link(node,group,0x14);leaves.extend(group)
 prior_root=[b+0xc000,b+0xc100];prior_mid=[b+0xc200];prior_leaf=[b+0xc300,b+0xc400]
 link(0x876f28,prior_root,0x28);link(0x876f58,prior_mid,0x1c);link(0x876f80,prior_leaf,0x14)
 for address,value in ((0x878e48,2),(0x878e4c,1),(0x878e50,2)):put(address,value)
 before=bytes(u.mem_read(b,0x10000));put(stack,stop);put(stack+4,root_node);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4cc010,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 assert walk(0x878e18,0x28)==[other]
 assert walk(0x876f28,0x28)==prior_root+[root_node]
 assert walk(0x876f58,0x1c)==prior_mid+mid
 assert walk(0x876f80,0x14)==prior_leaf+leaves
 assert walk(root_node+4,0x1c)==[]
 for node in mid:assert walk(node,0x14)==[]
 assert [get(a) for a in (0x878e48,0x878e4c,0x878e50)]==[3,1+len(mid),2+len(leaves)]
 # All payload and unrelated records remain byte-identical; only list links change.
 after=bytearray(u.mem_read(b,0x10000))
 for nodes,offset in (([root_node,other]+prior_root,0x28),([root_node+4]+mid+prior_mid,0x1c),(mid+leaves+prior_leaf,0x14)):
  for node in nodes:
   index=node+offset-b;after[index:index+8]=before[index:index+8]
 assert bytes(after)==before,case
 total_middle+=len(mid);total_leaf+=len(leaves)
report=dict(result='PASS',cases=256,middle_records=total_middle,leaf_records=total_leaf,original_sha256=digest,scope='Unmodified4cc010, no callbacks replaced. Synthetic well-formed ownership lists; exact FIFO recycling, emptied owner lists, preserved payload and free counters. No shared pool implementation, real allocation, rendering or XEMU gameplay claim.')
(root/'artifacts/player-resource-recycle-original.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

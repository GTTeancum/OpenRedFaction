"""Execute complete original skeletal destruction, slot removal and ring unlink.
No callees are replaced. The fixture supplies valid owner rings and motion tables.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
b=0x30000000;u.mem_map(b,0x20000);nodes=[b+i*0x2000 for i in range(4)];desc=b+0x9000;motions=b+0xb000;stack=b+0x1e000;stop=b+0x1f000;head=0x181bdb8
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
rng=random.Random(0x51b070);cases=0;removed=0
for count in range(17):
 for size in range(1,5):
  for victim in range(size):
   for enabled in (False,True):
    for duplicate in (False,True):
     raw=[bytearray(rng.randbytes(0x2000)) for _ in range(size)]
     for i in range(size):
      raw[i][0x1d54:0x1d5c]=w(nodes[(i+1)%size],nodes[(i-1)%size])
     ids=[i//2 if duplicate else i for i in range(16)];slots=[w(ids[i],rng.getrandbits(32),rng.getrandbits(32)) for i in range(16)]
     raw[victim][0x12d0:0x1394]=w(count)+b''.join(slots)
     raw[victim][0x1cfc:0x1d04]=w(3,5);raw[victim][0x1d48:0x1d4c]=w(7);raw[victim][0x1d50:0x1d54]=w(desc if enabled else 0)
     refs=[(0,1,2,99)[(i+cases)%4] for i in range(16)];expected_refs=refs[:]
     for i in range(16):u.mem_write(desc+0xf5c+4*i,w(motions+256*i));u.mem_write(motions+256*i+0x74,w(refs[i]))
     for i in range(size):u.mem_write(nodes[i],bytes(raw[i]))
     u.mem_write(head,w(nodes[0]));u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,nodes[victim]);u.emu_start(0x51b070,stop,count=100000)
     assert u.reg_read(UC_X86_REG_EIP)==stop
     expected_head=nodes[0]
     if enabled:
      raw[victim][0x1cfc:0x1d04]=w(-1,-1);raw[victim][0x1d48:0x1d4c]=w(-1)
      for n in range(count,0,-1):
       motion=struct.unpack('<I',slots[0][:4])[0];expected_refs[motion]=max(0,expected_refs[motion]-1)
       for i in range(n-1):slots[i]=slots[i+1]
      raw[victim][0x12d0:0x1394]=w(0)+b''.join(slots)
      if size==1:expected_head=0
      else:
       nxt=(victim+1)%size;prev=(victim-1)%size
       raw[prev][0x1d54:0x1d58]=w(nodes[nxt]);raw[nxt][0x1d58:0x1d5c]=w(nodes[prev])
       if victim==0:expected_head=nodes[nxt]
      raw[victim][0x1d54:0x1d5c]=w(0,0);removed+=count
     for i in range(size):assert bytes(u.mem_read(nodes[i],0x2000))==raw[i],(cases,i)
     assert read(head)==expected_head
     assert [read(motions+256*i+0x74) for i in range(16)]==expected_refs
     cases+=1
report=dict(result='PASS',cases=cases,removed_slots=removed,original_sha256=sha,scope=__doc__)
(root/'artifacts/skeletal-release-original.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

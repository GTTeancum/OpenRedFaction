"""Original corpse room collector through actual queue append and sphere culling."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(data)+4095)//4096*4096);u.mem_write(0x400000,data)
b=0x30000000;u.mem_map(b,65536);nodes=b;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
get=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
rng=random.Random(0x42e140);initial=bytes([0xa5])*(2048*48);appended=culled=mismatched=full=0
for case in range(288):
 count=case%9;room=(case//9)%4;queue_count=[0,2046,2048][(case//36)%3]
 offset=(float((case%3)-1),0.,0.);plane_count=(case//108)%2
 u.mem_write(0x88fd1c,b'\0');u.mem_write(0x9bb56c,w(0));u.mem_write(0x87bb00,struct.pack('<3f',*offset))
 # Keep sphere inside x<=0: plane rejects x+offset>extent.
 u.mem_write(0x1818a6c,struct.pack('<4fI',1,0,0,0,0)+bytes(8));u.mem_write(0x1818b8c,w(plane_count))
 expected=bytearray(initial);after_count=queue_count;original_nodes=bytearray()
 for i in range(8):
  descriptor=i%4;position=(float(i-4),float(i),0.);extent=[0.,.25,2.,8.][i%4]
  raw=bytearray(rng.randbytes(84));struct.pack_into('<5f3f',raw,0,1000,-999,5,.25,0.3,*position)
  struct.pack_into('<f',raw,4,extent);struct.pack_into('<3I',raw,68,descriptor,0xff123456,nodes+((i+1)%count)*84 if i<count else 0)
  struct.pack_into('<I',raw,80,nodes+((i+count-1)%count)*84 if i<count else 0);original_nodes.extend(raw)
  if i>=count:continue
  if descriptor!=room:mismatched+=1;continue
  if plane_count and position[0]+offset[0]>extent:culled+=1;continue
  if after_count==2048:full+=1;continue
  at=after_count*48;struct.pack_into('<I4f',expected,at,nodes+i*84,*position,extent)
  expected[at+20:at+25]=bytes([1,0,0,0,1]);struct.pack_into('<3I',expected,at+28,0,0,0);struct.pack_into('<I',expected,at+44,0x42df20)
  after_count+=1;appended+=1
 u.mem_write(nodes,bytes(original_nodes));u.mem_write(0x62f764,w(nodes if count else 0));u.mem_write(0x88fd20,initial)
 u.mem_write(0x9bb550,w(queue_count));u.mem_write(0x9bb568,w(0));u.mem_write(stack,w(stop,room))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x42e140,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 assert get(0x9bb550)==after_count and get(0x9bb568)==after_count-queue_count,(case,'counts')
 actual=bytes(u.mem_read(0x88fd20,len(initial)));assert actual==expected,(case,[(i,a,z) for i,(a,z) in enumerate(zip(actual,expected)) if a!=z][:16])
 assert bytes(u.mem_read(nodes,8*84))==original_nodes,(case,'effect mutation')
 if callable(globals().get('observe_case')):observe_case(count,room,queue_count,plane_count,offset,bytes(original_nodes),after_count,actual)
report=dict(result='PASS',cases=288,appended=appended,culled=culled,room_mismatches=mismatched,full_queue_rejections=full,original_sha256=digest,scope='Unhooked original42e140,4d3560,5186a0 and vector helpers. All active counts0..8, room identifiers including0, queue capacities at0/2046/2048 used slots, supplied world offsets and optional x plane. Exact2048 queue records and untouched effect payload/links. Culling uses existing extent, not elapsed-derived growth; callback is queued but not executed. No instance transform, live collector binding or GPU run.')
assert appended and culled and mismatched and full
(root/'artifacts/corpse-room-queue-original.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

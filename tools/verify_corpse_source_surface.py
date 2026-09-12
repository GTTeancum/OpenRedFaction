"""Full original42dc50 with synthetic room geometry and supplied attachment/color."""
import hashlib,itertools,json,math,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(data)+4095)//4096*4096);u.mem_write(0x400000,data)
b=0x30000000;u.mem_map(b,0x20000)
actor=b;model=b+0x2000;meta=b+0x2100;room=b+0x3000;world=b+0x4000
node=b+0x5000;face=b+0x6000;vertices=b+0x7000;edges=b+0x8000
slot=b+0x9000;stack=b+0x1d000;stop=b+0x1e000
identity=struct.pack('<9f',1,0,0,0,1,0,0,0,1)
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
get=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
put=lambda a,v:u.mem_write(a,w(v))
trace=[];origin=(0,0.5,0);query_snapshot=None

def hook(m,address,size,unused):
 global query_snapshot
 sp=m.reg_read(UC_X86_REG_ESP)
 def ret(value=0,pop=0):
  m.reg_write(UC_X86_REG_EAX,value&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,get(sp))
 if address==0x51d5b0:
  assert m.reg_read(UC_X86_REG_ECX)==meta
  ret(4,4)
 elif address==0x5034f0:
  assert tuple(get(sp+4+4*i) for i in range(4))==(model,4,actor+0x48,actor+0x3c)
  u.mem_write(get(sp+20),identity);u.mem_write(get(sp+24),struct.pack('<3f',*origin));ret()
 elif address==0x4df690:
  query,result,descriptor=(get(sp+4+4*i) for i in range(3))
  assert m.reg_read(UC_X86_REG_ECX)==world and descriptor==room
  assert tuple(struct.unpack('<3f',u.mem_read(query+0x34,12)))==origin
  assert struct.unpack('<3f',u.mem_read(query+0x40,12))==(0,-1,0)
  assert get(query+0x4c)==0 and get(query+0x50)==4 and get(result+4)==0x7f7fffff
  query_snapshot=(query,result)
  trace.append('query')
 elif address==0x4e5c60:
  out,hitface,point=(get(sp+4+4*i) for i in range(3))
  assert hitface==face
  actual=struct.unpack('<3f',u.mem_read(point,12))
  assert all(abs(a-b)<1e-5 for a,b in zip(actual,expected_point)),(actual,expected_point)
  trace.append('color')
  put(out,0xff123456);ret(out,12)

u.hook_add(UC_HOOK_CODE,hook)
results=[]
for slope,height,offset,existing,free_count,duration,size in itertools.product(
 [0,0.75],[-2,0,3],[0.25,0.75,2,-0.5],[0,1],[1,2],[5,8],[0.25,0.5]):
 origin=(0.5,height+slope*0.5+offset,0.25)
 expected_point=(origin[0],height+slope*origin[0],origin[2])
 expected_hit=0<offset<1
 normal=(-slope/math.sqrt(1+slope*slope),1/math.sqrt(1+slope*slope),0)
 other=slot+0x100;free_tail=slot+0x200
 u.mem_write(b,bytes(0x10000));trace.clear();query_snapshot=None
 put(actor,room);put(actor+0x80,model);put(model+8,meta)
 put(0x5a00f0,1);put(0x6460e8,world);put(world+0x74,1)
 put(0x62f488,slot);put(0x62f764,other if existing else 0)
 put(slot+0x4c,free_tail if free_count==2 else slot);put(slot+0x50,free_tail if free_count==2 else slot)
 put(free_tail+0x4c,slot);put(free_tail+0x50,slot)
 put(other+0x4c,other);put(other+0x50,other)
 bounds=struct.pack('<6f',-3,height-3*slope-0.0001,-3,3,height+3*slope+0.0001,3)
 u.mem_write(room+8,bounds);put(room+0x3c,node)
 u.mem_write(node,bounds+w(face,0,0,0));u.mem_write(face,struct.pack('<4f',*normal,-height*normal[1])+bounds)
 put(face+0x30,-1);put(face+0x40,edges)
 u.mem_write(vertices,struct.pack('<12f',-3,height-3*slope,-3,-3,height-3*slope,3,3,height+3*slope,3,3,height+3*slope,-3))
 for j in range(4):u.mem_write(edges+j*32,w(vertices+j*12)+bytes(16)+w(edges+(j+1)%4*32,edges+(j-1)%4*32))
 put(0xca06b0,15)
 u.mem_write(stack,w(stop,actor,0x595f18)+struct.pack('<2f',duration,size));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x42dc50,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 assert trace==(['query','color'] if expected_hit else ['query']),(origin,trace)
 if expected_hit:
  assert get(0x62f488)==(free_tail if free_count==2 else 0)
  assert get(0x62f764)==(other if existing else slot)
  assert get(slot+0x4c)==(other if existing else slot) and get(slot+0x50)==(other if existing else slot)
  if existing:assert get(other+0x4c)==slot and get(other+0x50)==slot
  if free_count==2:assert get(free_tail+0x4c)==free_tail and get(free_tail+0x50)==free_tail
  payload=struct.unpack('<17f2I',u.mem_read(slot,0x4c))
  assert payload[:4]==(0,0,duration,size)
  constant=struct.unpack('<f',u.mem_read(0x5895a8,4))[0]
  rate=struct.unpack('<f',struct.pack('<f',constant/duration))[0]
  assert payload[4]==rate
  assert all(abs(payload[5+i]-(expected_point[i]+normal[i]*0.01))<1e-5 for i in range(3)),payload
  assert all(abs(payload[14+i]-normal[i])<1e-6 for i in range(3))
  for row in range(3):
   assert abs(sum(payload[8+3*row+j]**2 for j in range(3))-1)<1e-5
  assert payload[17:]==(room,0xff123456)
 else:
  assert get(0x62f488)==slot and get(0x62f764)==(other if existing else 0)
  assert bytes(u.mem_read(slot,0x4c))==bytes(0x4c)
  assert get(slot+0x4c)==(free_tail if free_count==2 else slot)
  assert get(slot+0x50)==(free_tail if free_count==2 else slot)
 if 'observe_case' in globals():
  observe_case(dict(free_count=free_count,existing=existing,growth=duration,extent=size,
   matched=expected_hit,hit=bytes(u.mem_read(query_snapshot[1]+8,24)) if expected_hit else bytes(24),
   payload=bytes(u.mem_read(slot,76)),links=[get(a) for a in [0x62f488,0x62f764,slot+0x4c,slot+0x50,other+0x4c,other+0x50,free_tail+0x4c,free_tail+0x50]],
   geometry=bytes(u.mem_read(face,40))+bytes(u.mem_read(vertices,48))+struct.pack('<3f',*origin),
   addresses=[slot,other,free_tail]))
 results.append(dict(origin=origin,slope=slope,hit=expected_hit,existing=existing,free_count=free_count,duration=duration,size=size))
report=dict(result='PASS',original_sha256=digest,cases=len(results),hits=sum(x['hit'] for x in results),
 scope='Full original42dc50 with original room-tree geometry, vector/basis arithmetic and list publication. Attachment lookup/transform and face color are supplied boundaries. Flat/sloped single-face rooms; no reconstruction equivalence, authored geometry, lifetime or rendering.',examples=results[:4])
(root/'artifacts/corpse-source-surface.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

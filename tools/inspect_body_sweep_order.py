"""Execute original499ed0 to establish mover/world sphere-query ordering."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
source=root/'Installed_Game/RF.exe';digest=hashlib.sha256(source.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(source));data=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(data)+4095)//4096*4096);u.mem_write(0x400000,data)
base=0x30000000;u.mem_map(base,0x20000);out=base+0x10000;inputs=base+0x11000;body=base+0x12000;stack=base+0x1d000;stop=base+0x1e000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
identity=f(1,0,0,0,1,0,0,0,1);trace=[]
def observe(m,a,s,c):
 sp=m.reg_read(UC_X86_REG_ESP);query,result=struct.unpack('<2I',m.mem_read(sp+4,8))
 trace.append(dict(solid=(m.reg_read(UC_X86_REG_ECX)-base-0x400)//0x2000,start=list(struct.unpack('<3f',m.mem_read(query+0x34,12))),delta=list(struct.unpack('<3f',m.mem_read(query+0x40,12))),radius=struct.unpack('<f',m.mem_read(query+0x4c,4))[0],flags=struct.unpack('<I',m.mem_read(query+0x50,4))[0],limit=struct.unpack('<f',m.mem_read(result+4,4))[0]))
u.hook_add(UC_HOOK_CODE,observe,begin=0x4df1c0,end=0x4df1c0)
fixtures=[('mover only',[0,None,None],0,0),('later nearer',[0,4,None],0,1),('later farther',[4,0,None],0,0),('world nearer',[0,4,6],0,2),('world farther',[4,None,0],0,0),('equal mover',[0,0,None],0,1),('equal world',[0,None,0],0,2),('disabled mover',[4,0,None],0x40000,1),('static only',[None,None,0],0,2),('all miss',[None,None,None],0,None),('two spheres',[0,4,6],0,2)]
results=[]
for name,heights,first_flags,winner in fixtures:
 sphere_count=2 if name=='two spheres' else 1
 u.mem_write(base,bytes(0x6000))
 for i,z in enumerate(heights):
  block=base+i*0x2000;solid=block+0x400;face=block+0xa00;vertices=block+0xc00;edges=block+0xe00
  z=0 if z is None else z;bounds=f(-3,-3,z-.0001,3,3,z+.0001)
  u.mem_write(block+0x2c,w(100+i));u.mem_write(block+0x48,identity);u.mem_write(block+0x190,bounds);u.mem_write(block+0x294,w(solid));u.mem_write(block+0x28c,w(base+0x2000 if i==0 else 0x64e6e0));u.mem_write(block+0x7c,w(first_flags if i==0 else 0))
  u.mem_write(solid+0x70,w(face if heights[i] is not None else 0));u.mem_write(face,f(0,0,1,-z)+bounds);u.mem_write(face+0x30,w(-1));u.mem_write(face+0x40,w(edges));u.mem_write(vertices,f(-3,-3,z,3,-3,z,3,3,z,-3,3,z))
  for j in range(4):u.mem_write(edges+j*32,w(vertices+j*12)+bytes(16)+w(edges+((j+1)%4)*32,edges+((j-1)%4)*32))
 u.mem_write(0x64e96c,w(base));u.mem_write(0x6460e8,w(base+0x4400));u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,w(15))
 u.mem_write(body,bytes(0x300));u.mem_write(body+0x74,identity);u.mem_write(body+0xf8,f(.5));u.mem_write(body+0xfc,w(sphere_count,sphere_count,body+0x200));u.mem_write(body+0x124,w(0x460));u.mem_write(body+0x200,f(0,0,0,.5,0,0,1,0,0,.5,0,0));u.mem_write(inputs,f(0,0,8,0,0,-8));u.mem_write(out,bytes([0xa5])*68);u.mem_write(out+0x18,f(1));trace.clear()
 u.mem_write(stack,w(stop,inputs,inputs+12,body,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x499ed0,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;raw=bytes(u.mem_read(out,68));assert hit==int(winner is not None),(name,hit)
 if winner is not None:
  point_normal_fraction=struct.unpack('<7f',raw[:28]);expected=(sphere_count-1,0,heights[winner],0,0,1,(7.5-heights[winner])/16)
  assert point_normal_fraction==expected,(name,point_normal_fraction,expected)
  assert struct.unpack_from('<I',raw,48)[0]==(0xffffffff if winner==2 else 100+winner)
  assert struct.unpack_from('<I',raw,60)[0]==base+winner*0x2000+0xa00
 for query in trace:assert query['start'] in ([[0,0,8],[1,0,8]] if sphere_count==2 else [[0,0,8]]) and query['delta']==[0,0,-16] and query['radius']==.5 and query['flags']==0x464,(name,query)
 expected_order=[0,1,2]
 if name in ('later farther','world farther'):expected_order=[0,2]
 if name=='disabled mover':expected_order=[1,2]
 if sphere_count==2:expected_order=[0,0,1,1,2,2]
 assert [q['solid'] for q in trace]==expected_order,(name,trace)
 if sphere_count==2:assert [q['start'][0] for q in trace]==[0,1,0,1,0,1]
 results.append(dict(name=name,winner=winner,queries=list(trace),output=raw.hex()))
report=dict(result='PASS',cases=len(results),original_sha256=digest,scope='Full original499ed0 and geometry/material/transform callees, read-only query-entry hook. One/two spheres, two ordered movers and static zero-room plane solids; ordinary uncached query flags. Original execution only, no C/NXDK equivalence yet.',results=results)
(root/'artifacts/body-sweep-order.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
for result in results:print(result['name'],[(q['solid'],q['limit']) for q in result['queries']])

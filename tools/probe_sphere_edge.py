"""Pin original edge-sweep boundary behavior before its C reconstruction."""
import hashlib,json,math,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);out=base+4096;stack=base+60000;stop=base+64000
# Static vectors already initialized, as on every call after the first. Avoid
# unrelated CRT atexit registration; no query helper is replaced or skipped.
u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
trace=[]
for address,label in [(0x507512,'endpoint'),(0x50757f,'line candidate'),(0x5074a8,'negative-time clamp')]:
 u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:trace.append(data),user_data=label,begin=address,end=address)
a=(0,0,0);b=(0,2,0);endpoint_time=(2-math.sqrt(.1875))/4
fixtures=[
 ('edge interior',(-2,1,0),(4,0,0),.5,a,b,1,.375,(0,1,0)),
 ('exact tangent',(-2,1,.5),(4,0,0),.5,a,b,1,None,None),
 ('start endpoint',(-2,-.25,0),(4,0,0),.5,a,b,1,endpoint_time,a),
 ('far endpoint omitted',(-2,2.25,0),(4,0,0),.5,a,b,1,None,None),
 ('reversed endpoint',(-2,2.25,0),(4,0,0),.5,b,a,1,endpoint_time,b),
 ('equal limit rejected',(-2,1,0),(4,0,0),.5,a,b,.375,None,None),
 ('larger limit accepted',(-2,1,0),(4,0,0),.5,a,b,.3750001,.375,(0,1,0)),
 ('small negative entry',(-.49,1,0),(1,0,0),.5,a,b,1,1e-6,(0,1,0)),
 ('deep initial overlap',(0,1,0),(1,0,0),.5,a,b,1,None,None),
 ('parallel endpoint',(0,-2,0),(0,4,0),.5,a,b,1,.375,a),
 ('zero movement',(-2,1,0),(0,0,0),.5,a,b,1,None,None),
 ('zero edge',(-2,0,0),(4,0,0),.5,a,a,1,.375,a),
 ('zero radius',(-2,1,0),(4,0,0),0,a,b,1,None,None),
]
results=[]
for name,start,delta,radius,edge_a,edge_b,limit,fraction,point in fixtures:
 wire=struct.pack('<14f',*start,*delta,radius,*edge_a,*edge_b,limit);u.mem_write(base,wire);u.mem_write(out,bytes([0xa5])*16);trace.clear()
 u.mem_write(stack,struct.pack('<9I',stop,out+4,base,base+12,struct.unpack_from('<I',wire,24)[0],base+28,base+40,out,struct.unpack_from('<I',wire,52)[0]));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x5072e0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit==int(fraction is not None),(name,hit)
 actual=bytes(u.mem_read(out,16))
 expected=struct.pack('<4f',fraction,*point) if hit else bytes([0xa5])*16
 assert actual==expected,(name,actual.hex(),expected.hex())
 results.append(dict(name=name,hit=hit,output=actual.hex(),branches=list(trace)))
report=dict(result='PASS',cases=len(results),scope='Original steady-state 5072e0, all geometric callees unchanged. Static initialization flags seeded to avoid CRT atexit registration. Analytic boundary fixtures, not a C/NXDK implementation comparison.',results=results)
(root/'artifacts/sphere-edge-boundaries.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

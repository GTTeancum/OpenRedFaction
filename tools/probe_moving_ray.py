"""Full original moving/static ray wrapper: list order and fraction reuse."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,0x20000);out=base+0x10000;inputs=base+0x11000;stack=base+0x1d000;stop=base+0x1e000
identity=struct.pack('<9f',1,0,0,0,1,0,0,0,1);trace=[]
def observe(uc,address,size,data):
 sp=uc.reg_read(UC_X86_REG_ESP);query,result=struct.unpack('<2I',uc.mem_read(sp+4,8))
 trace.append(dict(solid=hex(uc.reg_read(UC_X86_REG_ECX)),start=struct.unpack('<3f',uc.mem_read(query+0x34,12)),delta=struct.unpack('<3f',uc.mem_read(query+0x40,12)),limit=struct.unpack('<f',uc.mem_read(result+4,4))[0]))
u.hook_add(UC_HOOK_CODE,observe,begin=0x4df1c0,end=0x4df1c0)
fixtures=[
 ('single mover',[0,None,None],0,0,.5,0),
 ('later closer outside retained limit',[0,2,None],0,0,.5,0),
 ('later mover equal retained limit',[0,4,None],0,1,.5,4),
 ('static after two shortenings',[0,4,6],0,2,.5,6),
 ('static closer outside retained limit',[0,None,2],0,0,.5,0),
 ('static equal retained limit',[0,None,4],0,2,.5,4),
 ('first mover exits before static',[0,4,6],1,0,.5,0),
 ('reversed movers',[4,0,None],0,0,.25,4),
 ('static only',[None,None,0],0,2,.5,0),
 ('no contacts',[None,None,None],0,None,None,None),
]
results=[]
for name,heights,first,winner,fraction,height in fixtures:
 u.mem_write(base,bytes(0x6000))
 for i,z in enumerate(heights):
  block=base+i*0x2000;solid=block+0x400;room=block+0x600;node=block+0x800;face=block+0xa00;v=block+0xc00;e=block+0xe00;array=block+0x1000
  z=0 if z is None else z;bounds=struct.pack('<6f',-3,-3,z-.0001,3,3,z+.0001)
  u.mem_write(block+0x2c,struct.pack('<I',100+i));u.mem_write(block+0x48,identity);u.mem_write(block+0xfc,identity);u.mem_write(block+0x190,bounds);u.mem_write(block+0x294,struct.pack('<I',solid))
  u.mem_write(block+0x28c,struct.pack('<I',base+0x2000 if i==0 else 0x64e6e0))
  u.mem_write(solid+0x90,struct.pack('<I',1));u.mem_write(solid+0x9c,struct.pack('<3I',1,1,array));u.mem_write(array,struct.pack('<I',room))
  u.mem_write(room+8,bounds);u.mem_write(room+0x3c,struct.pack('<I',node));u.mem_write(node,bounds+struct.pack('<4I',face if heights[i] is not None else 0,0,0,0))
  u.mem_write(face,struct.pack('<4f',0,0,1,-z)+bounds);u.mem_write(face+0x30,struct.pack('<i',-1));u.mem_write(face+0x40,struct.pack('<I',e))
  u.mem_write(v,struct.pack('<12f',-3,-3,z,3,-3,z,3,3,z,-3,3,z))
  for j in range(4):u.mem_write(e+j*32,struct.pack('<I',v+j*12)+bytes(16)+struct.pack('<II',e+((j+1)%4)*32,e+((j-1)%4)*32))
 u.mem_write(0x64e96c,struct.pack('<I',base));u.mem_write(0x6460e8,struct.pack('<I',base+0x4400));u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,struct.pack('<I',15))
 u.mem_write(inputs,struct.pack('<6f',0,0,8,0,0,-8));u.mem_write(out,bytes([0xa5])*68);trace.clear()
 u.mem_write(stack,struct.pack('<5I',stop,inputs,inputs+12,0x26|first,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x498e80,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit==int(winner is not None),(name,hit)
 raw=bytes(u.mem_read(out,68))
 if hit:
  actual=struct.unpack('<7f',raw[:28]);object_id=struct.unpack_from('<I',raw,48)[0];face_id=struct.unpack_from('<I',raw,60)[0]
  assert actual==(0,0,height,0,0,1,fraction),(name,actual)
  assert object_id==(0xffffffff if winner==2 else 100+winner) and face_id==base+winner*0x2000+0xa00,(name,object_id,hex(face_id))
 else:assert raw==bytes([0xa5])*68
 results.append(dict(name=name,hit=hit,output=raw.hex(),queries=list(trace)))
report=dict(result='PASS',cases=len(results),scope='Complete original 498e80 and all callees unchanged, two moving list entries and one static solid, identity poses, texture index -1 avoids resource lookup. Geometry and object/face identities checked. Boundary evidence, not a C/NXDK wrapper comparison.',results=results)
(root/'artifacts/moving-ray-boundaries.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

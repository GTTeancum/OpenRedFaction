"""Execute original corpse surface draw; observe only final polygon submission."""
import hashlib,json,math,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(data)+4095)//4096*4096);u.mem_write(0x400000,data)
b=0x30000000;u.mem_map(b,65536);node=b;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
get=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
observed=[]
def hook(m,address,size,unused):
 if address!=0x517110:return
 sp=m.reg_read(UC_X86_REG_ESP);texture,count,vertices,uv,state,color=[get(sp+4+i*4) for i in range(6)]
 assert texture==123 and count==4 and state==456
 observed.append((bytes(m.mem_read(vertices,48)),bytes(m.mem_read(uv,32)),get(color)))
 m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,get(sp))
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(42);rows=[];max_error=0
for case in range(512):
 growth=rng.choice([5,8]);elapsed=f32(rng.choice([0,growth-0.00001,growth,growth+0.00001,1000,rng.random()*growth]))
 maximum=rng.choice([0.25,0.5]);rate=f32(f32(math.pi/2)/growth)
 position=[f32(rng.uniform(-100,100)) for _ in range(3)]
 basis=[f32(rng.uniform(-1,1)) for _ in range(9)]
 color=rng.getrandbits(32)
 raw=struct.pack('<17f4I',elapsed,-999,growth,maximum,rate,*position,*basis,99,color,0x12345678,0x76543210)
 u.mem_write(node,raw);u.mem_write(0x62f73c,w(123));u.mem_write(0x17c7c58,w(456));u.mem_write(stack,w(stop,node))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);observed.clear();u.emu_start(0x42df20,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and len(observed)==1
 after=bytes(u.mem_read(node,84));assert after[:4]==raw[:4] and after[8:]==raw[8:]
 extent=struct.unpack_from('<f',after,4)[0]
 expected_extent=f32(math.sin(elapsed*rate)*maximum) if elapsed<growth else maximum
 error=abs(extent-expected_extent);max_error=max(max_error,error);assert error<=6e-8,(case,extent,expected_extent)
 vertices,uv,actual_color=observed[0];expected=[]
 # Original scales each basis vector, then spills center +/- first before second.
 for sign_a,sign_b in [(-1,1),(1,1),(1,-1),(-1,-1)]:
  for axis in range(3):
   a=f32(basis[axis]*extent);bb=f32(basis[3+axis]*extent)
   expected.append(f32(f32(position[axis]+sign_a*a)+sign_b*bb))
 assert vertices==struct.pack('<12f',*expected),(case,struct.unpack('<12f',vertices),expected)
 assert uv==struct.pack('<8f',0,0,1,0,1,1,0,1) and actual_color==(color|0xff000000)
 if callable(globals().get("observe_case")):observe_case(raw,after,vertices+uv+w(actual_color))
 rows.append(dict(elapsed=elapsed,growth=growth,extent=extent))
report=dict(result='PASS',cases=len(rows),original_sha256=digest,max_libm_extent_difference=max_error,scope='Full original42df20 executes x87 sine and unchanged vector helpers; only517110 polygon submission is supplied. Exact vertex float bytes, UVs, forced opaque RGB and extent-only node mutation verified. Growth compared to independent double libm with6e-8 absolute bound, not bit-equivalence. Synthetic positions/bases, both authored growth parameters, boundary and mature cases. No shared draw implementation, GPU rendering or live dispatch claim.')
(root/'artifacts/corpse-surface-draw-original.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

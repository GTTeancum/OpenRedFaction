"""Compare full original 558e30 geometry with shared PC and NXDK helpers."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
result=None
def hook(m,address,size,data):
 global result
 sp=m.reg_read(UC_X86_REG_ESP)
 if address==0x515b40:
  args=struct.unpack('<4I',m.mem_read(sp+4,16));assert args[0]==base and args[1]==0
  result=struct.pack('<2I',0,1)+bytes(80)
 else:
  bitmap,count,positions,uv,mode=struct.unpack('<5I',m.mem_read(sp+4,20));assert count==4 and mode==0x118c42
  result=struct.pack('<2I',0,0)+b''.join(bytes(m.mem_read(positions+i*12,12))+bytes(m.mem_read(uv+i*8,8)) for i in range(4))
 m.reg_write(UC_X86_REG_EIP,stop)
for address in (0x515b40,0x558d40):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(55830);commands=bytearray();results=bytearray();fallbacks=0
entry=int(re.search(r'_rf_particle_stretch_build\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for case in range(2048):
 position=[rng.randint(-128,128)/8 for _ in range(3)]
 delta=([0,0,0],[.031,0,0],[.032,0,0],[0,0,3],[1,2,3],[(rng.random()-.5)*8 for _ in range(3)])[case%6]
 previous=[a+b for a,b in zip(position,delta)]
 forward=((0,0,1),(0,0,.125),(0,0,0),(1,0,0),(.3,-.5,.9))[case%5]
 radius=(case%17)/8
 command=struct.pack('<10f',*position,*previous,*forward,radius);commands.extend(command)
 u.mem_write(base,command);u.mem_write(0x18186e0,command[24:36]);u.mem_write(stack,struct.pack('<IIIfI',stop,base,base+12,radius,0x118c42));u.reg_write(UC_X86_REG_ESP,stack)
 result=None;u.emu_start(0x558e30,stop,count=100000);assert result is not None
 results.extend(result);fallbacks+=struct.unpack_from('<I',result,4)[0]
 x.mem_write(base,command);x.mem_write(base+0x100,bytes(84));x.mem_write(stack,struct.pack('<IIIIfII',stop,base,base+12,base+24,radius,base+0x104,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,84))
 assert actual==result,(case,struct.unpack('<10f',command),struct.unpack('<2I20f',actual),struct.unpack('<2I20f',result))
actual=subprocess.check_output([str(c['probe']),'--particle-stretch'],input=commands)
assert actual==results,[(i//88,i%88,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
report=dict(result='PASS',cases=2048,fallbacks=fallbacks,scope='Full original 558e30, all vector helpers unchanged; only final 558d40 polygon submission and 515b40 billboard fallback intercepted. Exact world-space vertex/UV bytes on PC and NXDK. Zero/parallel forward, threshold sides, static/moving positions and varied radii. Projection, clipping and live rendering not covered.')
(root/'artifacts/particle-stretch-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

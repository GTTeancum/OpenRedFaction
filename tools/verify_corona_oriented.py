"""Original5590f0 endpoint clipping/quad construction versus PC and compiled NXDK."""
import runpy,struct,random,re,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_corona_oriented_build\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
result=None

def hook(m,address,size,data):
 global result
 sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<10I',m.mem_read(sp,40))
 if address==0x515b40:
  assert args[1]==base+24 and args[2]==0 and args[3]==struct.unpack('<I',command[48:52])[0] and args[4]==0x06010c41
  result=struct.pack('<II',0,1)+bytes(80)
 else:
  assert args[1:3]==(0x123,4) and args[5]==0x06010c41
  assert tuple(v&255 for v in args[6:10])==(11,22,33,44)
  positions=bytes(m.mem_read(args[3],48));uv=bytes(m.mem_read(args[4],32))
  result=struct.pack('<II',0,2)+b''.join(positions[i*12:i*12+12]+uv[i*8:i*8+8] for i in range(4))
 m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
for a in (0x558d40,0x515b40):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
rng=random.Random(5590);commands=bytearray();results=bytearray();counts={}
fixtures=[]
for i in range(1024):
 camera=tuple(rng.randint(-64,64)/4 for _ in range(3))
 forward=((0,0,1),(1,0,0),(0,1,0),(.6,0,.8))[i%4]
 a=tuple(camera[j]+rng.randint(-128,128)/8 for j in range(3))
 b=a if i%16==0 else tuple(camera[j]+rng.randint(-128,128)/8 for j in range(3))
 fixtures.append((*camera,*forward,*a,*b,(i%17)/4))
for a,b in (((0,0,4),(0,0,8)),((0,0,.16),(2,0,.16)),((0,0,0),(0,0,0)),((0,0,0),(.03125,0,0))):
 fixtures.append((0,0,0,0,0,1,*a,*b,2))
# Float neighbors at the near plane and squared-distance fallback threshold.
for bits in (0x3e23d709,0x3e23d70a,0x3e23d70b):
 z=struct.unpack('<f',struct.pack('<I',bits))[0]
 for a,b in (((0,0,z),(2,0,4)),((2,0,4),(0,0,z)),((0,0,z),(2,0,z))):
  fixtures.append((0,0,0,0,0,1,*a,*b,2))
for bits in (0x3d0186e1,0x3d0186e2,0x3d0186e3):
 d=struct.unpack('<f',struct.pack('<I',bits))[0]
 fixtures.append((0,0,0,0,0,1,0,0,4,d,0,4,2))
for case,values in enumerate(fixtures):
 command=struct.pack('<13f',*values);commands.extend(command)
 u.mem_write(base,command);u.mem_write(0x1818690,command[:12]);u.mem_write(0x18186e0,command[12:24])
 u.mem_write(0x17c7c18,struct.pack('<I',0x123));u.mem_write(0x17c7c14,bytes((11,22,33,44)))
 u.mem_write(stack,struct.pack('<III',stop,base+24,base+36)+command[48:]+struct.pack('<I',0x06010c41));u.reg_write(UC_X86_REG_ESP,stack)
 result=bytes(88);u.emu_start(0x5590f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 results.extend(result);kind=struct.unpack_from('<I',result,4)[0];counts[kind]=counts.get(kind,0)+1
 x.mem_write(base,command);x.mem_write(base+256,bytes(84))
 x.mem_write(stack,struct.pack('<5I',stop,base,base+12,base+24,base+36)+command[48:]+struct.pack('<II',base+260,base+256));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+256,84))
 assert actual==result,(case,values,struct.unpack('<II20f',actual),struct.unpack('<II20f',result))
actual=subprocess.check_output([str(c['probe']),'--corona-oriented'],input=commands)
assert actual==results,[(i//88,i%88,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:12]
guards=[]
for field in range(13):
 values=list(fixtures[1]);values[field]=float('nan');command=struct.pack('<13f',*values);guards.append(command)
 x.mem_write(base,command);x.mem_write(base+256,bytes([0xa5])*84)
 x.mem_write(stack,struct.pack('<5I',stop,base,base+12,base+24,base+36)+command[48:]+struct.pack('<II',base+260,base+256));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(base+256,84))==bytes([0xa5])*84
actual=subprocess.check_output([str(c['probe']),'--corona-oriented'],input=b''.join(guards))
assert actual==(struct.pack('<i',-4)+bytes(84))*len(guards)
report=dict(result='PASS',cases=len(fixtures),guards=len(guards),kinds=counts,scope='Full original5590f0, only final558d40 and fallback515b40 intercepted; exact output quad/UV and fallback identity. No final clipping, GPU pixels or scene scheduling claim.')
(root/'artifacts/corona-oriented-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

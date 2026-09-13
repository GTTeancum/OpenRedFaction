"""Execute original camera normalization, dot, acos and volume fade together."""
import runpy,struct,re,random,subprocess,json,math
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EDI
entry=int(re.search(r'_rf_glare_volume_camera_opacity\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
rng=random.Random(414278);commands=bytearray();results=bytearray();fixtures=[]
for axis in range(3):
 for sign in (-1,1):
  for cone in (0,25,90,180):
   forward=[0,0,0];forward[axis]=sign
   fixtures.append([0,0,0]+forward+[3,4,5]+[cone])
for _ in range(2000):
 # Keep the direction inside the acos domain; separate guards cover invalid input.
 forward=[rng.uniform(-1,1) for _ in range(3)]
 length=math.sqrt(sum(v*v for v in forward))
 fixtures.append([rng.uniform(-100,100) for _ in range(3)]+[v/length*.999 for v in forward]+[rng.uniform(-100,100) for _ in range(3)]+[rng.uniform(0,180)])
for values in fixtures:
 command=struct.pack('<10f',*values);commands.extend(command)
 u.mem_write(base+0x103c,command[:12]);u.mem_write(base+0x1060,command[12:24]);u.mem_write(stack+0x20,command[24:36]);u.mem_write(base+0x2010,command[36:])
 u.mem_write(stack+0x60,b'\x01');u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,base+0x1000);u.reg_write(UC_X86_REG_ESI,base+0x2000)
 u.emu_start(0x414278,0x414307,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x414307
 expected=bytes(4)+bytes(u.mem_read(stack+12,4))+w(u.mem_read(stack+0x60,1)[0]);results.extend(expected)
 x.mem_write(base,command);x.mem_write(base+0x1000,bytes([0xa5])*8);x.mem_write(stack,w(stop,base,base+12,base+24)+command[36:]+w(base+0x1000,base+0x1004));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(4)+bytes(x.mem_read(base+0x1000,8))==expected,(values,expected.hex(),bytes(x.mem_read(base+0x1000,8)).hex())
assert subprocess.check_output([str(c['probe']),'--glare-volume-camera'],input=commands)==results
guards=[]
for i in range(10):
 value=[0,0,0,0,0,1,3,4,5,45];value[i]=float('nan');guards.append(value)
guards += [[0,0,0,0,0,1,0,0,0,45],[0,0,0,0,0,2,0,0,1,45]]
for values in guards:
 command=struct.pack('<10f',*values);x.mem_write(base,command);x.mem_write(base+0x1000,bytes([0xa5])*8)
 x.mem_write(stack,w(stop,base,base+12,base+24)+command[36:]+w(base+0x1000,base+0x1004));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffe and bytes(x.mem_read(base+0x1000,8))==bytes([0xa5])*8
 assert subprocess.check_output([str(c['probe']),'--glare-volume-camera'],input=command)==w(0xfffffffe)+bytes([0xa5])*8
report=dict(result='PASS',cases=len(fixtures),guards=len(guards),scope='Original414278..414307 with unmodified subtraction,4faaf0 normalization,negation,dot,CRT acos and clamp. Exact PC/NXDK opacity/draw. Guards are shared-code policy for undefined/domain-invalid original inputs; parent/actor gates and scene integration excluded.')
(root/'artifacts/glare-volume-camera.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

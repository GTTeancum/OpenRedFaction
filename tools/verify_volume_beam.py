"""Original555b80 beam world vertices with explicit transform/submission boundaries."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_volume_beam_build\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
result=b'';transforms=0

def hook(m,a,size,data):
 global result,transforms
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x518360:
  m.mem_write(read(sp+4),bytes(m.mem_read(read(sp+8),12)));transforms+=1
 else:
  assert read(sp+4)==4 and read(sp+12)==1 and read(sp+16)==0x12345678
  ptrs=[read(read(sp+8)+i*4) for i in range(4)]
  result=b''.join(bytes(m.mem_read(p,12))+bytes(m.mem_read(p+28,8)) for p in ptrs)
 m.reg_write(UC_X86_REG_EIP,read(sp));m.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x518360,0x5159a0):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x50be40,stop,count=10000)
assert read(0x1775b30)==0x06110c42
rng=random.Random(55580);commands=bytearray();results=bytearray()
for i in range(1024):
 values=[rng.uniform(-128,128) for _ in range(10)]
 if i%8==0:values[3:9]=[0,0,0,0,0,0]
 if i%8==1:values[:9]=[0,0,0,0,0,8,0,0,-8]
 if i%8==2:values[9]=0
 command=struct.pack('<10f',*values);commands.extend(command);u.mem_write(0x1818690,command[:12]);u.mem_write(base,command)
 u.mem_write(stack,w(stop,base+12,base+24)+command[36:]+w(0x12345678));u.reg_write(UC_X86_REG_ESP,stack);result=b'';transforms=0
 u.emu_start(0x555b80,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and len(result)==80 and transforms==4
 expected=result;results.extend(bytes(4)+expected)
 x.mem_write(base,command);x.mem_write(base+0x1000,bytes([0xa5])*80);x.mem_write(stack,w(stop,base,base+12,base+24)+command[36:]+w(base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 actual=bytes(x.mem_read(base+0x1000,80));assert actual==expected,(i,struct.unpack('<20f',actual),struct.unpack('<20f',expected))
assert subprocess.check_output([str(c['probe']),'--volume-beam'],input=commands)==results
for i in range(10):
 command=bytearray(commands[:40]);command[i*4:i*4+4]=struct.pack('<f',float('nan'));x.mem_write(base,bytes(command));x.mem_write(base+0x1000,bytes([0xa5])*80);x.mem_write(stack,w(stop,base,base+12,base+24)+bytes(command[36:])+w(base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffe and bytes(x.mem_read(base+0x1000,80))==bytes([0xa5])*80
 assert subprocess.check_output([str(c['probe']),'--volume-beam'],input=command)==w(0xfffffffe)+bytes([0xa5])*80
report=dict(result='PASS',cases=1024,guards=10,scope='Full555b80 world geometry with unchanged vector math/normalization.518360 captures world points and5159a0 captures four ordered UV vertices. Exact PC/compiled NXDK; zero length, collinear, zero/negative widths. Projection/clipping, render mode and live/native drawing excluded.')
(root/'artifacts/volume-beam.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

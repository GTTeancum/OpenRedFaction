"""Original 5477a0 point projection, including cached flags and reciprocal bias."""
import runpy,struct,re,itertools,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u=c['u'];x=c['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_particle_project\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=bytearray();expected=bytearray();rng=random.Random(5477);cases=0
for clamp,flags,z,offset in itertools.product((0,1,256,257),(0,1,2,3,4,128),(-2,-0.1,0,0.1,1,1.99999988,2,2.00000024,10),(0,0.1,-0.1)):
 center=(rng.randint(-1000,1000)/64,rng.randint(-1000,1000)/64,z);projection=struct.pack('<I3f2i',clamp,offset,320,240,-32,17);point=struct.pack('<6f4B',*center,99,-77,123,0xa5,flags,0x5a,0x6b)
 u.mem_write(base,point);u.mem_write(0x5a445a,bytes([clamp&255]));u.mem_write(0x1e652e8,struct.pack('<f',offset));u.mem_write(0x1818a5c,struct.pack('<f',320));u.mem_write(0x1818a24,struct.pack('<f',240));u.mem_write(0x17c7bec,struct.pack('<ii',-32,17));u.mem_write(stack,struct.pack('<II',stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x5477a0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=bytes(4)+bytes(u.mem_read(base,28));expected.extend(result);commands.extend(projection+point)
 x.mem_write(base,projection+point);x.mem_write(stack,struct.pack('<III',stop,base,base+24));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+24,28))==result,(clamp,flags,z,offset)
 cases+=1
assert subprocess.check_output([str(c['probe']),'--particle-project'],input=commands)==expected
report=dict(result='PASS',cases=cases,x87_control_word='0x027f',scope='Unchanged original 5477a0 versus PC/NXDK. Cached flags, clamp/behind-camera behavior, zero Z and reciprocal-only offset threshold. All point bytes exact; polygon clipping and rasterization excluded.')
(root/'artifacts/particle-projection-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

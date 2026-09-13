"""Original volume angular fade after acos, with unmodified clamp."""
import runpy,struct,re,random,subprocess,json,math
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI
entry=int(re.search(r'_rf_glare_volume_opacity\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
rng=random.Random(4142);commands=bytearray();results=bytearray();fixtures=[]
for cone in (0,10,25,45,90,180):
 for delta in (-26,-25,-24.607844,-24.607843,-24.607842,0,25,26):fixtures.append(((cone+delta)/57.2957763671875,cone))
fixtures += [(rng.random()*math.pi,rng.uniform(0,180)) for _ in range(2000)]
for angle,cone in fixtures:
 command=struct.pack('<df',angle,cone);commands.extend(command)
 u.mem_write(base,command);u.mem_write(base+0x1010,command[8:]);u.mem_write(base+0x2000,b'\xdd\x05'+w(base))
 u.emu_start(base+0x2000,base+0x2006,count=1);u.mem_write(stack+0x60,b'\x01');u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base+0x1000)
 u.emu_start(0x4142af,0x414307,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x414307
 expected=bytes(4)+bytes(u.mem_read(stack+12,4))+w(u.mem_read(stack+0x60,1)[0]);results.extend(expected)
 x.mem_write(base+0x1000,bytes([0xa5])*8);x.mem_write(stack,w(stop)+command+w(base+0x1000,base+0x1004));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(4)+bytes(x.mem_read(base+0x1000,8))==expected,(angle,cone,expected.hex())
assert subprocess.check_output([str(c['probe']),'--glare-volume-opacity'],input=commands)==results
for angle,cone in [(float('nan'),0),(float('inf'),0),(0,float('nan')),(0,float('inf'))]:
 command=struct.pack('<df',angle,cone);x.mem_write(base+0x1000,bytes([0xa5])*8);x.mem_write(stack,w(stop)+command+w(base+0x1000,base+0x1004));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffe and bytes(x.mem_read(base+0x1000,8))==bytes([0xa5])*8
 assert subprocess.check_output([str(c['probe']),'--glare-volume-opacity'],input=command)==w(0xfffffffe)+bytes([0xa5])*8
report=dict(result='PASS',cases=len(fixtures),guards=4,scope='Original4142af..414307 after resolved acos, actual40a4c0 clamp. Exact PC/compiled NXDK float opacity and draw gate. Includes near cutoff cases; camera, parent/actor gates, beam geometry and native rendering excluded.')
(root/'artifacts/glare-volume-opacity.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Original48770f tag publication with actual48a230 and matrix/vector copies."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_glare_publish_tag_pose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
u.mem_write(0x64ecb9,bytes(2));rng=random.Random(48770);commands=bytearray();results=bytearray()
fields=[(152,0x3c,12),(288,0xe4,12),(300,0xf0,12),(448,0x190,24),(164,0x48,36),(312,0xfc,36),(348,0x120,36)]
for i in range(1024):
 flags=rng.getrandbits(32);radius=(-1,0,.25,1,32)[i%5];pose=tuple(rng.randint(-4096,4096)/16 for _ in range(12))
 command=w(flags)+struct.pack('<13f',radius,*pose);commands.extend(command)
 original=bytearray([0xa5]*748);original[0x7c:0x80]=w(flags);original[0x180:0x184]=struct.pack('<f',radius);u.mem_write(base,bytes(original))
 u.mem_write(stack+8,command[44:56]);u.mem_write(stack+20,command[8:44]);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,base)
 u.emu_start(0x48770f,0x487750,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x487750
 after=bytes(u.mem_read(base,748));result=bytes(4)+after[0x7c:0x80]+b''.join(after[a:a+n] for _,a,n in fields);results.extend(result)
 expected=bytearray([0xa5]*528);expected[120:124]=after[0x7c:0x80];expected[444:448]=command[4:8]
 owner=bytearray(expected);owner[120:124]=w(flags)
 for target,source,n in fields:expected[target:target+n]=after[source:source+n];original[source:source+n]=after[source:source+n]
 original[0x7c:0x80]=after[0x7c:0x80];assert after==original
 x.mem_write(base,bytes(owner));x.mem_write(base+1024,command[8:]);x.mem_write(stack,w(stop,base,base+1024));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 actual=bytes(x.mem_read(base,528));assert actual==expected,(i,[(j,a,b) for j,(a,b) in enumerate(zip(actual,expected)) if a!=b][:15])
actual=subprocess.check_output([str(c['probe']),'--glare-tag-publish'],input=commands);assert actual==results
# Nonfinite resolved pose must not partially update the owner.
bad_commands=bytearray()
for field in range(12):
 pose=[0.0]*12;pose[field]=float('nan');owner=bytes(expected);x.mem_write(base,owner);x.mem_write(base+1024,struct.pack('<12f',*pose));x.mem_write(stack,w(stop,base,base+1024));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffe and bytes(x.mem_read(base,528))==owner
 bad_commands.extend(w(flags)+struct.pack('<13f',1,*pose))
actual=subprocess.check_output([str(c['probe']),'--glare-tag-publish'],input=bad_commands)
assert actual==(w(0xfffffffe,flags)+bytes([0xa5])*168)*12
report=dict(result='PASS',cases=1024,guards=12,scope='Actual48770f..487750, unhooked48a230 and all vector/matrix copies with resolved tag pose. Full original/compiled NXDK owner preservation; PC selected fields. Parent/tag lookup, negative tags, recursive ordering and scene integration excluded.')
(root/'artifacts/glare-tag-publish.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

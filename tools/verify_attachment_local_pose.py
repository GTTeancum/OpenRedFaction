"""Original negative-tag transform with unmodified matrix inverse and multiplication."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_attachment_local_pose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda v:struct.pack('<'+'f'*len(v),*v)
rng=random.Random(48766);commands=bytearray();results=bytearray();singular=0
for i in range(1536):
 values=[rng.uniform(-8,8)*2**rng.randrange(-8,9) for _ in range(33)]
 if i%8==0:values[24:33]=[1,0,0,0,1,0,0,0,1]
 if i%8==1:values[24:33]=[0]*9;singular+=1
 if i%8==2:values[24:33]=[1,2,3,2,4,6,0,0,0];singular+=1
 command=f(values);commands.extend(command)
 u.mem_write(base,bytes(0x900));u.mem_write(base+0x1000,bytes(0x900));u.mem_write(base+0x204,w(0xffffffff))
 u.mem_write(base+0x208,command[36:48]);u.mem_write(base+0x214,command[:36])
 u.mem_write(base+0x1000+0xf0,command[48:60]);u.mem_write(base+0x1000+0xfc,command[60:96]);u.mem_write(base+0x1000+0x120,command[96:132])
 u.mem_write(stack,w(stop,base,base+0x1000));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x487630,0x48770f,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==0x48770f
 sp=u.reg_read(UC_X86_REG_ESP);expected=bytes(u.mem_read(sp+20,36))+bytes(u.mem_read(sp+8,12));results.extend(bytes(4)+expected)
 x.mem_write(base,command);x.mem_write(base+0x1000,bytes([0xa5])*48);x.mem_write(stack,w(stop,base,base+48,base+60,base+96,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 actual=bytes(x.mem_read(base+0x1000,48));assert actual==expected,(i,struct.unpack('<12f',actual),struct.unpack('<12f',expected))
 assert bytes(x.mem_read(base,132))==command
actual=subprocess.check_output([str(c['probe']),'--attachment-local-pose'],input=commands);assert actual==results
bad=bytearray()
for i in range(33):
 command=bytearray(commands[:132]);command[i*4:i*4+4]=f([float('nan')]);bad.extend(command)
 x.mem_write(base,bytes(command));x.mem_write(base+0x1000,bytes([0xa5])*48);x.mem_write(stack,w(stop,base,base+48,base+60,base+96,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffe and bytes(x.mem_read(base+0x1000,48))==bytes([0xa5])*48
assert subprocess.check_output([str(c['probe']),'--attachment-local-pose'],input=bad)==(w(0xfffffffe)+bytes([0xa5])*48)*33
report=dict(result='PASS',cases=1536,singular_cases=singular,nonfinite_guards=33,scope='Original487630 negative-tag prefix through48770f with unmodified inverse4fc930/4fccf0, determinant4fc4c0, multiplication40ea80 and vector helpers. Exact PC/compiled NXDK pose bytes, preserved inputs and guard outputs. No parent lookup, pose publication, live integration or native scene replay.')
(root/'artifacts/attachment-local-pose.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

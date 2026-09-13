"""Original actor volume aim geometry with actual vector helpers."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,B,S,STOP=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*(int(v)&0xffffffff for v in v))
entry=int(re.search(r'_rf_glare_volume_actor_aim\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def halt(m,a,size,ctx):
 if a==0x41441e:m.emu_stop()
u.hook_add(UC_HOOK_CODE,halt)
identity=[1,0,0,0,1,0,0,0,1];fixtures=[];rng=random.Random(4143)
for length in (0,.000099999,.0001,.000100001,.001,1):
 for axis in range(3):
  vector=[0,0,0];vector[axis]=length;fixtures.append(vector+[0,0,0]+identity)
fixtures += [[rng.uniform(-10,10) for _ in range(3)]+[rng.uniform(-10000,10000) for _ in range(3)]+[rng.uniform(-2,2) for _ in range(9)] for _ in range(1000)]
commands=bytearray();results=bytearray();rejected=0
for values in fixtures:
 command=struct.pack('<15f',*values);commands.extend(command)
 u.mem_write(S+0x1c,command[:12]);u.mem_write(S+0x28,command[48:60]);u.mem_write(B+0x103c,command[12:60]);u.reg_write(UC_X86_REG_EBP,B+0x1000);u.reg_write(UC_X86_REG_ESP,S)
 u.emu_start(0x4143b7,0x414406,count=100000);end=u.reg_read(UC_X86_REG_EIP);assert end in (0x414406,0x41441e)
 if end==0x414406:
  u.mem_write(B+0x3000,b'\xdd\x1d'+w(B+0x4000));u.emu_start(B+0x3000,B+0x3006,count=1);expected=bytes(4)+bytes(u.mem_read(B+0x4000,8))+w(1)
 else:expected=bytes(16);rejected+=1
 results.extend(expected)
 x.mem_write(B,command);x.mem_write(B+0x4000,bytes([0xa5])*12);x.mem_write(S,w(STOP,B,B+12,B+24,B+0x4000,B+0x4008));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x4000,12));assert actual==expected,(values,actual.hex(),expected.hex())
assert subprocess.check_output([str(c['probe']),'--volume-actor-aim'],input=commands)==results
guards=[]
for i in range(15):
 value=[1,2,3,0,0,0]+identity;value[i]=float('nan');guards.append(value)
guards += [[1,2,3]+[0]*12,[1,1,1]+[1e30]*3+identity]
for values in guards:
 command=struct.pack('<15f',*values);x.mem_write(B,command);x.mem_write(B+0x4000,bytes([0xa5])*12)
 x.mem_write(S,w(STOP,B,B+12,B+24,B+0x4000,B+0x4008));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
 expected=w(-2)+bytes([0xa5])*12
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x4000,12))==expected
 assert subprocess.check_output([str(c['probe']),'--volume-actor-aim'],input=command)==expected
report=dict(result='PASS',original_cases=len(fixtures),short_vector_rejections=rejected,guards=len(guards),scope='Original4143b7..414406/41441e, unmodified length, world transform, position subtraction, normalization and dot. Exact PC/NXDK double dot and eligibility; guards are shared policy. Actor714 selection and branch eligibility remain caller-owned.')
(root/'artifacts/volume-actor-aim.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Original resolved actor aim-dot sizing, including real CRT random state."""
import runpy,struct,re,random,subprocess,json,math
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,B,S,STOP=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI
w=lambda *v:struct.pack('<'+'I'*len(v),*(int(v)&0xffffffff for v in v))
entry=int(re.search(r'_rf_glare_volume_actor_dimensions\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
fixtures=[];rng=random.Random(4144)
for length in (.01,.3,1,2,8):
 for dot in (-1,-.000001,0,.3/length-1e-8,.3/length,.3/length+1e-8,1):
  for seed in (0,1,0xffffffff):
   for draw in (0,1):fixtures.append((dot,length,2,seed,draw))
fixtures += [(rng.uniform(-1,1),rng.uniform(.01,20),rng.uniform(.01,20),rng.getrandbits(32),rng.randrange(2)) for _ in range(2000)]
commands=bytearray();results=bytearray()
for dot,length,width,seed,draw in fixtures:
 command=struct.pack('<dffII',dot,length,width,seed,draw);commands.extend(command)
 u.mem_write(B,command[:8]);u.mem_write(B+0x2028,command[12:16]+command[8:12]);u.mem_write(c['thread']+0x14,w(seed))
 u.mem_write(S+0x10,command[8:12]);u.mem_write(S+0x18,command[12:16]);u.mem_write(S+0x68,bytes([draw]))
 u.mem_write(B+0x3000,b'\xdd\x05'+w(B));u.emu_start(B+0x3000,B+0x3006,count=1)
 u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_ESI,B+0x2000);u.emu_start(0x414406,0x41441e,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==0x41441e
 expected=bytes(4)+bytes(u.mem_read(c['thread']+0x14,4))+bytes(u.mem_read(S+0x10,4))+bytes(u.mem_read(S+0x18,4))+w(u.mem_read(S+0x68,1)[0]);results.extend(expected)
 x.mem_write(B,command);x.mem_write(B+0x4000,bytes([0xa5])*8);x.mem_write(S,w(STOP)+command[:16]+w(B+16,B+0x4000,B+20));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+16,4))+bytes(x.mem_read(B+0x4000,8))+bytes(x.mem_read(B+20,4))
 assert actual==expected,(dot,length,width,seed,actual.hex(),expected.hex())
assert subprocess.check_output([str(c['probe']),'--volume-actor-dimensions'],input=commands)==results
guards=[(float('nan'),1,1),(float('inf'),1,1),(1,float('nan'),1),(1,1,float('inf')),(1,0,1),(1e300,1,1)]
for dot,length,width in guards:
 command=struct.pack('<dffII',dot,length,width,123,1);x.mem_write(B,command);x.mem_write(B+0x4000,bytes([0xa5])*8)
 x.mem_write(S,w(STOP)+command[:16]+w(B+16,B+0x4000,B+20));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
 expected=w(-2,123)+bytes([0xa5])*8+w(1)
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+16,4))+bytes(x.mem_read(B+0x4000,8))+bytes(x.mem_read(B+20,4))
 assert actual==expected and subprocess.check_output([str(c['probe']),'--volume-actor-dimensions'],input=command)==expected
report=dict(result='PASS',original_cases=len(fixtures),guards=len(guards),scope='Original414406 negative gate and4144bd sizing with unmodified504e40/504db0/57312d CRT randomness. Exact dimensions, draw suppression and RNG state PC/NXDK. Invalid-input guards are shared policy. Actor lookup, aim geometry, branch eligibility and live actor binding excluded.')
(root/'artifacts/volume-actor-dimensions.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

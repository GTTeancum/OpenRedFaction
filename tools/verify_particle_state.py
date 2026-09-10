"""Particle_State ordered UID actions against complete original methods."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
x.mem_map(base+0x10000,0x40000);state=base+0x10000
entry=int(re.search(r'_rf_level_particles_set_state\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(3949);commands=bytearray();results=bytearray()
for case in range(512):
 action=case%2;now=(0,1072800000,13,99999)[case%4];count=case%9
 ids=[11,12,11,14];links=[rng.choice([11,12,14,99,0xffffffff]) for i in range(8)]
 enabled=[0xaabbcc00|rng.choice([0,1,2,255]) for i in range(4)];deadlines=[rng.choice([-1,0,333]) for i in range(4)]
 command=w(action,now,count,*links,*[v for i in range(4) for v in (ids[i],enabled[i],deadlines[i])]);commands.extend(command)
 before=bytearray(b'\xa5'*0x600)
 for i in range(4):
  struct.pack_into('<I',before,i*0x160,ids[i]);struct.pack_into('<I',before,i*0x160+0x140,enabled[i]);struct.pack_into('<i',before,i*0x160+0x154,deadlines[i])
 u.mem_write(base+0x1000,bytes(before));u.mem_write(base+0x29c,w(count,8,base+0x800));u.mem_write(base+0x800,w(*links))
 u.mem_write(0x646080,w(4,4,base+0x900));u.mem_write(base+0x900,w(*[base+0x1000+i*0x160 for i in range(4)]));u.mem_write(0x5a3ed8,w(now))
 u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4b94f0 if action else 0x4ba270,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytes(u.mem_read(base+0x1000,len(before)));expected=w(0)+b''.join(after[i*0x160+0x140:i*0x160+0x144]+after[i*0x160+0x154:i*0x160+0x158] for i in range(4));results.extend(expected)
 for i in range(len(before)):
  if i%0x160 not in (0x140,0x154,0x155,0x156,0x157):assert before[i]==after[i]
 raw=bytearray(222300)
 for i in range(4):
  a=192000+i*228;struct.pack_into('<I',raw,a+172,enabled[i]);struct.pack_into('<i',raw,a+152,deadlines[i]);struct.pack_into('<I',raw,a+208,ids[i]);struct.pack_into('<I',raw,a+224,1)
 x.mem_write(state,bytes(raw));x.mem_write(base,w(state,0,base+0x300,0,4,0,0,0));x.mem_write(base+0x300,w(*[v for uid in ids for v in (uid,0)]));x.mem_write(base+0x800,w(*links))
 x.mem_write(stack,w(stop,base,base+0x800,count,action,now));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+b''.join(bytes(x.mem_read(state+192000+i*228+172,4))+bytes(x.mem_read(state+192000+i*228+152,4)) for i in range(4))
 assert actual==expected,(case,actual.hex(),expected.hex())
 actual_state=bytearray(x.mem_read(state,len(raw)))
 for j in range(4):
  a=192000+j*228;actual_state[a+152:a+156]=raw[a+152:a+156];actual_state[a+172]=raw[a+172]
 assert actual_state==raw

actual=subprocess.check_output([str(c['probe']),'--particle-state'],input=commands);assert actual==results
report=dict(result='PASS',cases=512,scope='Complete original 4b94f0/4ba270, 45d630 UID lookup, 4973b0/4973d0 and timer calls; no intercepted callees. Exact PC/NXDK enabled/deadline bytes, original surrounding state and full native state preservation. Missing IDs, repeated links, duplicate UID first-match, noncanonical enabled bytes and timer endpoints. Event scheduler integration excluded.')
(root/'artifacts/particle-state-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

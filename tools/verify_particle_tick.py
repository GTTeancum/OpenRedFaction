"""Emitter clock and emission decision vs original 4972f0."""
import runpy,struct,re,json,subprocess,random,itertools
from pathlib import Path
ctx=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')));root=ctx['root'];u=ctx['u'];x=ctx['x'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop'];thread=ctx['thread'];probe=ctx['probe']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
entry=int(re.search(r'_rf_particle_emitter_tick\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
observed=[0,0,0];due=0
calls=[]
def hook(m,a,s,d):
 if a==0x496f60:observed[0]+=1;calls.append('phase');return
 if a not in (0x4fa3f0,0x496c50,0x40a0e0):return
 value=0
 if a==0x4fa3f0:observed[1]+=1;value=due;calls.append('timer')
 elif a==0x496c50:observed[2]+=1;calls.append('emit')
 else:calls.append('parent')
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0];m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret);m.reg_write(UC_X86_REG_EAX,value)
u.hook_add(UC_HOOK_CODE,hook);commands=bytearray();expected=bytearray();rng=random.Random(4972);toggled=emitted=0
fixtures=itertools.product((0,1,2,256,257),(0,4,32,36),(0,1,2,255,256,257,0xa5a50000,0xa5a50001),(0,0.25,-0.25,10),(0,1,2,256))
for case,(global_enabled,flags,enabled,dt,due) in enumerate(fixtures):
 cycle=(0.5,0.25,1.0,0.5);elapsed=(case%7)/4;duration=(case%3)/2;seed=rng.getrandbits(32)
 observed[:]=[0,0,0];calls.clear();u.mem_write(base,bytes(0x158));u.mem_write(base+0x128,struct.pack('<4f',*cycle));u.mem_write(base+0x38,struct.pack('<I',flags));u.mem_write(base+0x138,struct.pack('<ffI',duration,elapsed,enabled));u.mem_write(thread+0x14,struct.pack('<I',seed));u.mem_write(0x59fd1c,bytes([global_enabled&255]));u.mem_write(0x5a4014,struct.pack('<f',dt));u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x4972f0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and max(observed)<=1
 if global_enabled&255:assert calls[-1]=='parent'
 else:assert not calls
 assert calls==sorted(calls,key=lambda k:['phase','timer','emit','parent'].index(k))
 result=bytes(4)+bytes(u.mem_read(thread+0x14,4))+struct.pack('<I',flags)+bytes(u.mem_read(base+0x140,4))+bytes(u.mem_read(base+0x13c,4))+bytes(u.mem_read(base+0x138,4))+struct.pack('<3I',*observed)
 expected.extend(result);toggled+=observed[0];emitted+=observed[2]
 command=struct.pack('<4fIfII IIff',*cycle,global_enabled,dt,due,seed,flags,enabled,elapsed,duration);commands.extend(command)
 x.mem_write(base,command);x.mem_write(base+0x100,bytes([0xa5])*12);x.mem_write(stack,struct.pack('<III f IIII',stop,base,global_enabled,dt,due,base+28,base+32,base+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(4)+bytes(x.mem_read(base+28,20))+bytes(x.mem_read(base+0x100,12))==result,case
assert subprocess.check_output([str(probe),'--particle-tick'],input=commands)==expected
report=dict(result='PASS',cases=case+1,toggles=toggled,emissions=emitted,scope='Original 4972f0 with actual phase-duration/RNG callees. Timer query supplied, emission intercepted without mutation, parent lookup returns null. PC/NXDK clock, decisions, call order and RNG match; live particle emission, parent motion and timer reset excluded.')
(root/'artifacts/particle-tick-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Original4154a0 orphan marking with unmodified generation-aware40a0e0."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_glare_parent_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
rng=random.Random(4154);commands=bytearray();results=bytearray();marked=0
for i in range(1024):
 flags=rng.getrandbits(32);handle=(0xffffffff,0,0x10001,0x752e03ff,0x1000400,0xffff0000,0x12340200,0xabcdef12)[i%8]
 stored=handle if i%3 else handle^0x10000;present=(i//3)%2;attached=rng.getrandbits(32)
 command=w(flags,handle,stored,present,attached);commands.extend(command);slot=handle&0xffff
 original=bytearray([0xa5]*748);original[0x30:0x34]=w(handle);original[0x7c:0x80]=w(flags);original[0x200:0x204]=w(attached)
 u.mem_write(base,bytes(original));u.mem_write(0x7394cc,bytes(4096));u.mem_write(base+0x1000+0x2c,w(stored))
 if slot<1024:u.mem_write(0x7394cc+slot*4,w(base+0x1000 if present else 0))
 u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4154a0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytes(u.mem_read(base,748));result=after[0x7c:0x80];original[0x7c:0x80]=result;assert after==original
 marked+=result!=w(flags);results.extend(bytes(4)+result)
 owner=bytearray([0xa5]*528);owner[0:4]=w(attached);owner[120:124]=w(flags);owner[128:132]=w(handle)
 x.mem_write(base,bytes(owner));x.mem_write(base+0x2000,bytes(12300))
 if slot<1024:x.mem_write(base+0x2000+slot*8,w(base+0x1000 if present else 0,stored))
 x.mem_write(stack,w(stop,base,base+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 owner[120:124]=result;assert bytes(x.mem_read(base,528))==owner
assert subprocess.check_output([str(c['probe']),'--glare-parent'],input=commands)==results
report=dict(result='PASS',cases=1024,changed_flags=marked,scope='Original4154a0 plus real40a0e0 lookup, exact PC flags and compiled NXDK full owner preservation. Absent sentinel, missing object, stale generation, valid and out-of-range slots; unrelated attachment parent randomized. No deletion scheduling, unlink, native replay or parent destruction.')
(root/'artifacts/glare-parent.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

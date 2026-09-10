"""Original46bae0 complete translation reversal versus PC/NXDK."""
import json,random,re,runpy,struct,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
entry=int(re.search(r'_rf_group_translation_reverse\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
fields=[(0x318,4),(0x2e8,4),(0x2f8,8),(0x300,4),(0x30c,4),(0x2f4,4),(0x304,4),(0x310,4),(0x7c,4),(0xe4,12),(0xf0,12),(0x144,12)]
rng=random.Random(0x46bae0);commands=bytearray();expected=bytearray()
for case in range(4096):
 flags=rng.choice([0,1,2,0x2000,0x2003,0x80002002]);current=case%2;target=1-current
 if case%13==0:target=-1
 if case%17==0:current=-1
 points=[rng.uniform(-100,100) for i in range(6)]
 if case%19==0:points[3:]=points[:3]
 timings=[[rng.choice([0.,.5,1.,-1.,2.]) for i in range(5)] for j in range(2)]
 runtime=w(flags,2,current,target)+struct.pack('<f',rng.uniform(-1,10))+w(-1)+struct.pack('<2f',rng.uniform(0,10),rng.uniform(-100,400))+w(12345,0x12345678)+struct.pack('<9f',*(rng.uniform(-20,20) for i in range(9)))
 keywire=b''.join(struct.pack('<8f',*points[i*3:i*3+3],*timings[i]) for i in range(2));commands.extend(runtime+keywire)
 original=bytearray([0xa5]*1024);at=0
 for offset,n in fields:original[offset:offset+n]=runtime[at:at+n];at+=n
 original[0x29c:0x2a8]=w(2,2,base+0x2000);u.mem_write(base,bytes(original));u.mem_write(base+0x2000,w(base+0x1000,base+0x1080))
 for i in range(2):
  raw=bytearray(128);raw[4:16]=keywire[i*32:i*32+12];raw[0x34:0x48]=keywire[i*32+12:i*32+32];u.mem_write(base+0x1000+i*128,bytes(raw))
 u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x46bae0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 raw=bytes(u.mem_read(base,1024));want=b''.join(raw[a:a+n] for a,n in fields);expected.extend(w(0)+want)
 x.mem_write(base,runtime)
 for i in range(2):
  key=bytearray(356);key[24:36]=keywire[i*32:i*32+12];key[72:92]=keywire[i*32+12:i*32+32];x.mem_write(base+0x1000+i*356,bytes(key))
 x.mem_write(stack,w(stop,base,base+0x1000,2));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 actual=bytes(x.mem_read(base,76));assert actual==want,(case,actual.hex(),want.hex())
 # Only mapped fields may change in the original full object.
 for a,n in fields:original[a:a+n]=raw[a:a+n]
 assert bytes(original)==raw,(case,'extra mutation')
pc=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--group-reverse'],input=commands);assert pc==expected
report=dict(result='PASS',cases=4096,scope='Full original46bae0 and real key/length/clamp/vector helpers. Exact PC/NXDK full runtime; whole original object mutation checked. Forward/reverse, idle indices, zero length, negative timing, distance below/above segment, retained poses/deadline, cleared speed/velocity. Obstruction decision and live motion excluded.')
(root/'artifacts/group-reverse-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

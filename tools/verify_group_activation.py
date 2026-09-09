"""Controller activation state transition against unchanged original instructions."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
source=root/'Installed_Game/RF.exe';assert hashlib.sha256(source.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(0x30000000,65536);return u
u=machine(source);base=0x30000000;stack=base+50000;vector=base+4096
offsets=[0x318,0x2e8,0x2f8,0x2fc,0x300,0x30c]
rng=random.Random(0x46ac43);cases=[];expected=[]
for n in range(4096):
 count=rng.randrange(2,129);flags=(n&0xfff)|rng.choice([0,0x2000,0x80000000,0x80002000]);mode=n%6
 if flags&4 and n%3==0:count=1
 current=rng.choice([0,count-1,rng.randrange(count)]);next_key=-1 if n%4 else rng.randrange(count)
 phase=rng.choice([0.,-0.,.25,-2.5,1e20]);terminal=rng.choice([-1,0,count-1])
 state=struct.pack('<2I2ifi',flags,mode,current,next_key,phase,terminal)
 before=bytearray([0xa5]*1024)
 for i,offset in enumerate(offsets):before[offset:offset+4]=state[i*4:i*4+4]
 u.mem_write(base,bytes(before));u.mem_write(vector,struct.pack('<I',count));u.mem_write(stack+0x340,struct.pack('<I',0xffffffff))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base);u.reg_write(UC_X86_REG_EDI,vector)
 stop=0x46acb2 if next_key==-1 else 0x46af8d
 u.emu_start(0x46ac43,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytes(u.mem_read(base,1024));result=b''.join(after[o:o+4] for o in offsets)
 for offset in offsets:before[offset:offset+4]=after[offset:offset+4]
 assert bytes(before)==after,(n,'unexpected object mutation')
 cases.append(struct.pack('<I',count)+state);expected.append(struct.pack('<i',0)+result)
# Port validation is separate from original behavior; no state mutation on error.
for count,current,flags in [(0,0,4),(1,0,0),(2,-1,0),(2,2,0),(0x80000000,0,4)]:
 state=struct.pack('<2I2ifi',flags,1,current,-1,.5,5);cases.append(struct.pack('<I',count)+state);expected.append(struct.pack('<i',-4)+state)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-activate'],input=b''.join(cases));assert len(pc)==28*len(cases)
for n,want in enumerate(expected):assert pc[n*28:n*28+28]==want,(n,'PC')
x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'_rf_group_motion_activate\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);stop=base+64000
for n,wire in enumerate(cases):
 x.mem_write(base,wire);count,=struct.unpack_from('<I',wire);x.mem_write(stack,struct.pack('<3I',stop,base+4,count));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+4,24));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',original_cases=4096,port_guards=5,scope='Original 46ac43 state transition through 46acb2/46af8d, unchanged gate/count/handle helpers; both directions, wraparound, mode 1 terminal keys, bit-4 branch, phase reset and active no-op. Whole object mutation checked. PC and NXDK match. Eligibility, sounds/events, object wakeup and pose playback are outside this block.')
(root/'artifacts/group-activation-verification.json').write_text(json.dumps(report,indent=2));print(report)

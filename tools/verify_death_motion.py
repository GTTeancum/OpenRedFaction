"""Original death-animation stage vs shared PC and NXDK orchestration."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
b=0x30000000;stack=b+0x1d000;stop=b+0x1e000;base=b+0x4000;effective=b+0x6000;pose=b+0xb000;thunk=b+0x3000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,0x20000);return m
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_death_motion_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def get(m,a):return struct.unpack('<I',m.mem_read(a,4))[0]
def ret(m,v):
 sp=m.reg_read(UC_X86_REG_ESP);target=get(m,sp);m.reg_write(UC_X86_REG_EAX,v&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,target)
trace=[];facts=[];pose_calls=0;flags=bytearray();mode='original'
def operation(m,op,first,second):
 global pose_calls
 trace.extend((op,first,second))
 if op==0:return facts[1]
 if op==1:return facts[2]
 if op==3:
  value=facts[3+(pose_calls!=0)];pose_calls+=1;return value
 if op==4:
  assert first==pose and second<50;flags[second]=0
 if op==5:
  address=b+0x810 if mode=='original' else b
  m.mem_write(address,w(get(m,address)^facts[5]))
 return 0
addresses={0x420c00:0,0x40a1e0:1,0x503400:2,0x5034d0:3,0x428c90:5}
def original_hook(m,a,size,data):
 if a==0x4895d0:ret(m,facts[0]);return
 if a not in addresses:return
 op=addresses[a];sp=m.reg_read(UC_X86_REG_ESP)
 if op in (0,1):assert get(m,sp+4)==b;first=second=0
 elif op in (2,3):first=get(m,sp+4);second=0
 else:
  assert get(m,sp+4)==b and get(m,sp+12)==0x3f800000 and get(m,sp+20)==1
  first=get(m,sp+8);second=get(m,sp+16)&255
 ret(m,operation(m,op,first,second))
def write_hook(m,access,a,size,value,data):
 if pose+0x13bc<=a<pose+0x13bc+50*48 and (a-pose-0x13bc)%48==0:
  assert size==1 and value==0;operation(m,4,pose,(a-pose-0x13bc)//48)
def native_hook(m,a,size,data):
 if a==thunk:
  sp=m.reg_read(UC_X86_REG_ESP);ret(m,operation(m,get(m,sp+8),get(m,sp+12),get(m,sp+16)))
u.hook_add(UC_HOOK_CODE,original_hook);u.hook_add(UC_HOOK_MEM_WRITE,write_hook);x.hook_add(UC_HOOK_CODE,native_hook)
rng=random.Random(0x41ff19);commands=[];expected=[];counts=dict(player=0,skipped=0,unavailable=0,played=0,ordinary=0,special=0)
for case in range(2048):
 state=[rng.getrandbits(32),rng.choice([-1,-1,0,5,14,15,44]),rng.randrange(-1,45),rng.choice([0,b+0xa000]),rng.getrandbits(32),
        *[rng.choice([-1,0,1,2,7,49]) for _ in range(3)],rng.randrange(50),rng.randrange(50),rng.getrandbits(32),rng.getrandbits(32),
        *[rng.choice([-1,0,8,21]) for _ in range(45)]]
 # Most calls exercise animation, with explicit high-byte-only predicate cases.
 if case%4:state[0]&=~0x80
 facts=[rng.choice([0,0,0,256,1,255]),rng.choice([-1,0,5,14,15,44]),rng.choice([0,256,1,255]),pose,rng.choice([0,pose]),rng.getrandbits(32)]
 if not facts[2]&255:facts[3]=rng.choice([0,pose])
 initial_flags=rng.randbytes(52);mode='original';trace=[];pose_calls=0;flags=bytearray(initial_flags)
 u.mem_write(b,bytes(0x1c000));u.mem_write(stack,bytes(0x200));u.mem_write(0x6fc4d8,b'\0');u.mem_write(0x64ecb9,b'\0')
 locations=[b+0x810,b+0x83c,b+0x824,b+0x80,base+0x724,*[base+0x13d8+4*i for i in range(3)],
            effective+0x13d8,effective+0x13dc,b+0x1464,b+0x1468,*[b+0xa54+16*i for i in range(45)]]
 for address,value in zip(locations,state):u.mem_write(address,w(value))
 u.mem_write(b+0x294,w(base));u.mem_write(b+0x29c,w(effective))
 for i,v in enumerate(initial_flags[:50]):u.mem_write(pose+0x13bc+i*48,bytes([v]))
 body=bytes(u.mem_read(b,0x1500));u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x41feed,0x4200a0,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4200a0
 result=b''.join(bytes(u.mem_read(a,4)) for a in locations);original_trace=trace[:];original_flags=bytes(flags)
 unchanged=bytearray(u.mem_read(b,0x1500))
 for off in (0x810,0x824,0x1464,0x1468):unchanged[off:off+4]=body[off:off+4]
 assert unchanged==body
 for i in range(50):assert bytes(u.mem_read(pose+0x13bc+i*48,1))==original_flags[i:i+1]
 if facts[0]&255:counts['player']+=1
 elif state[0]&0x80:counts['skipped']+=1
 elif 5 not in original_trace[::3]:counts['unavailable']+=1
 else:
  counts['played']+=1;counts['special' if state[4]&0x200000 else 'ordinary']+=1
 mode='native';trace=[];pose_calls=0;flags=bytearray(initial_flags)
 x.mem_write(b,w(*state));x.mem_write(b+0x2000,w(thunk,0));x.mem_write(stack,w(stop,b,facts[0],b+0x2000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b,228))==result,(case,'state')
 assert trace==original_trace and bytes(flags)==original_flags,(case,'callbacks',trace,original_trace)
 commands.append(w(*state,*facts)+initial_flags)
 expected.append(w(0)+result+original_flags+w(len(trace)//3)+w(*trace)+bytes((48-len(trace))*4))
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--death-motion'],input=b''.join(commands))==b''.join(expected)
# Port guards: invalid selected actions preserve all state; a missing pose
# after reset reports failure without clearing the actor's override words.
guards=[]
for requested,selected,skeletal,first_pose in ((45,5,1,pose),(-2,5,1,pose),(-1,45,1,pose),(5,5,1,0)):
 state=[0,requested,7,b+0xa000,0,0,1,2,3,4,123,456]+[1]*45
 facts=[0,selected,skeletal,first_pose,pose,0];initial_flags=bytes([123])*52
 mode='native';trace=[];pose_calls=0;flags=bytearray(initial_flags)
 x.mem_write(b,w(*state));x.mem_write(b+0x2000,w(thunk,0));x.mem_write(stack,w(stop,b,0,b+0x2000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(b,228))==w(*state)
 want=w(-4)+w(*state)+initial_flags+w(len(trace)//3)+w(*trace)+bytes((48-len(trace))*4)
 got=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--death-motion'],input=w(*state,*facts)+initial_flags)
 assert got==want;guards.append(requested)

report=dict(result='PASS',cases=len(commands),port_guards=len(guards),branches=counts,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original SP41feed..42009f instructions including direct bone-byte clears, compared with PC/NXDK state and callback sequence. Supplied player/type/selection/reset/pose/play helper boundaries. Play mutates flags to verify pre/post ordering. Full death entry, real animation playback and live dispatch excluded.')
(root/'artifacts/death-motion.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

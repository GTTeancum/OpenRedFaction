"""Controller sound selection and handle retention against original 46a120."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x30000000;stack=base+0xe000;stop=base+0xf000;callback=base+0xf100
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(im)+4095)//4096*4096);m.mem_write(b,im);m.mem_map(base,65536);return m
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=machine(original);x=machine(root/'build/xbox/main.exe');traces={}
entry=int(re.search(r'_rf_group_sound_start\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def audio(m,address,size,original_call):
 sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<7I',m.mem_read(sp,28))
 if original_call:
  ret,sample,position,volume,unused,flags=args[:6];assert unused==0x173c378
 else:ret,context,sample,position,volume,flags=args[:6]
 words=(sample,*struct.unpack('<3I',m.mem_read(position,12)),volume,flags)
 trace=traces[original_call];trace[0]+=1
 for value in words:trace[1]=((trace[1]^value)*16777619)&0xffffffff
 trace[2].append(words)
 m.reg_write(UC_X86_REG_EAX,0xffffffff if sample==0xffffffff else sample^0x12340000)
 m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,audio,user_data=True,begin=0x5056a0,end=0x5056a0)
x.hook_add(UC_HOOK_CODE,audio,user_data=False,begin=callback,end=callback)
rng=random.Random(0x46a120);commands=bytearray();expected=bytearray();calls=reverse=0
for n in range(2048):
 samples=[rng.choice([-1,0,rng.randrange(1,1024)]) for _ in range(4)];handles=[rng.getrandbits(32) for _ in range(4)]
 flags=rng.getrandbits(32);next_key=rng.choice([-1,0,1,2,9]);position=struct.pack('<3f',*(rng.uniform(-100,100) for _ in range(3)))
 state=w(*samples,*handles);wire=state+w(flags,next_key)+position;commands.extend(wire)
 before=bytearray([0xa5]*0x400);before[0x2d8:0x2e8]=state[:16];before[0x31c:0x32c]=state[16:]
 before[0x318:0x31c]=w(flags);before[0x2fc:0x300]=w(next_key);before[0x3c:0x48]=position
 u.mem_write(base,bytes(before));u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);traces[True]=[0,2166136261,[]]
 u.emu_start(0x46a120,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytes(u.mem_read(base,0x400));before[0x31c:0x32c]=after[0x31c:0x32c];assert bytes(before)==after
 want=w(0)+state[:16]+after[0x31c:0x32c]+w(*traces[True][:2]);expected.extend(want)
 x.mem_write(base,wire);x.mem_write(stack,w(stop,base,flags,next_key,base+40,callback,0));x.reg_write(UC_X86_REG_ESP,stack);traces[False]=[0,2166136261,[]]
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,32))+w(*traces[False][:2]);assert got==want,('NXDK',n)
 assert traces[False]==traces[True];calls+=traces[True][0];reverse+=after[0x328:0x32c]!=state[28:32]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-sound-start'],input=commands);assert actual==expected
report=dict(result='PASS',cases=2048,play_requests=calls,reverse_handle_updates=reverse,original_sha256=digest,scope='Original 46a120 and flag predicates execute unchanged; only 5056a0 audio backend intercepted. Exact PC/NXDK sample order, position, volume=1, flags=0 and returned handle retention, including missing samples/backend failure. Full original object mutation checked. Sample loading, audible backend, stopping and live activation excluded.')
(root/'artifacts/group-sound-start-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

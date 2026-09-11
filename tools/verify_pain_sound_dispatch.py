"""Original pain sample selection and playback wrapper; device calls supplied."""
import hashlib,json,random,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xf000;thread=b+0x6000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_audio_group_choose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];draws=0;predicate=0;player=0
def hook(m,address,size,context):
 global draws
 if address not in (0x577eef,0x48acf0,0x40d740,0x505560,0x5056a0):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<8I',m.mem_read(sp,32));result=0
 if address==0x577eef:draws+=1;result=thread
 elif address==0x48acf0:assert a[1]==b;result=predicate
 elif address==0x40d740:assert a[1]==123;result=player
 elif address==0x505560:trace.append(('flat',a[1:5]));result=456
 else:trace.append(('spatial',(a[1],bytes(m.mem_read(a[2],12)).hex(),a[3],a[4],a[5])));result=789
 m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
def run(m,at,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(at,stop,count=100000)
 assert m.reg_read(UC_X86_REG_EIP)==stop;return m.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x434da0);valid=0
for i in range(1024):
 count=(-1,0,1,2,3,16)[i%6];group=(-1,0,1,2147483647)[(i//6)%4];seed=rng.getrandbits(32)
 samples=[rng.randrange(-1,4096) for _ in range(16)];draws=0
 u.mem_write(0x636ef8,w(1));u.mem_write(0x63011c,w(count,b+0x4000));u.mem_write(b+0x4000,w(*samples));u.mem_write(thread+20,w(seed))
 got=run(u,0x434da0,[group]);after=struct.unpack('<I',u.mem_read(thread+20,4))[0]
 if group!=0:assert got==0xffffffff and after==seed and draws==0;continue
 valid+=1;assert draws==int(count>1)
 x.mem_write(b,w(1,0,0,0,0,0,0,0));x.mem_write(b+0x4000,w(*samples));x.mem_write(b+0x5000,w(seed))
 assert run(x,entry,[b,b+0x4000,16,count,b+0x5000,b+0x5100])==0
 assert bytes(x.mem_read(b+0x5100,4))==w(got) and bytes(x.mem_read(b+0x5000,4))==w(after)
for i in range(256):
 predicate=(0,1,256,257)[i%4];player=(0,1,0xffffffff)[i%3];trace=[]
 obj=bytearray(rng.randbytes(0x1500));obj[0x1430:0x1434]=w(b+0x7000)
 u.mem_write(b,bytes(obj));u.mem_write(b+0x70c4,w(123))
 position=w(0x3f800000,0xc0000000,0x40400000);sample=rng.randrange(-1,4096)
 run(u,0x48a9c0,[b,*struct.unpack('<3I',position),sample,0x3f000000,0x3e800000])
 expected=('flat',(sample&0xffffffff,0,0x3e800000,0x3f000000)) if predicate&255 and player==0 else ('spatial',(sample&0xffffffff,position.hex(),0x3f800000,0x173c378,0))
 assert trace==[expected],(i,trace,expected)
 assert bytes(u.mem_read(b,len(obj)))==obj,'wrapper changed entity, including voice808'
report=dict(result='PASS',selection_cases=1024,linked_nxdk_selection_cases=valid,dispatch_cases=256,
 scope='Full original434da0 with actual CRT RNG and supplied TLS address; valid selections agree with linked NXDK rf_audio_group_choose. Full48a9c0 with supplied predicates and device calls: exact routing/arguments and untouched entity bytes. Does not establish device playback or caller-owned eye position updates.')
(root/'artifacts/pain-sound-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

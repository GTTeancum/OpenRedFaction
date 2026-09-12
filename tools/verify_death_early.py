"""Original SP41fe5f..41feed local-player checks and global death timers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v));b=0x30000000;stack=b+0xe000;stop=b+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
u=machine(original);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_death_early_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def player(index,native):return (b+0x1000+(index-1)*8 if native else b+index*0x2000) if index else 0
def index(ptr,native):return ((ptr-b-0x1000)//8+1 if native else (ptr-b)//0x2000) if ptr else 0
facts=[];trace=[]
def hook(m,a,size,data):
 native=m is x;sp=m.reg_read(UC_X86_REG_ESP)
 if native:
  if a!=b+0x5000:return
  op=get(m,sp+8);token=get(m,sp+12);local=b+4;current=b+8
 else:
  if a not in (0x48c9f0,0x4ace90,0x4ad8a0):return
  op=(0x48c9f0,0x4ace90,0x4ad8a0).index(a);ptr=get(m,sp+4);token=1 if op==0 else index(ptr,False);local=0x5cb054;current=0x7c75d4
 trace.extend((op,token))
 if op==0 and facts[1]==1:m.mem_write(local,w(1 if native else b))
 if op==1 and facts[1]==2:m.mem_write(current,w(player(2,native)))
 if op==1 and facts[1]==3:m.mem_write(local,w(99 if native else b+0x6000))
 if op==1 and facts[1]==4:m.mem_write(current,w(0))
 if op==2 and facts[1]==5:m.mem_write(current,w(player(2,native)))
 target=get(m,sp);m.reg_write(UC_X86_REG_EAX,facts[0] if op==1 else 0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,target)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x41fe5f);commands=[];expected=[];stops=0
for case in range(1024):
 state=[1,rng.choice([1,99]),rng.choice([0,1,2]),rng.choice([0,77,1072798500,1072799250,1072800000]),rng.randrange(1072800001),rng.randrange(1072800001),1,rng.getrandbits(32),2,rng.getrandbits(32)]
 facts=[rng.choice([0,256,1,255]),case%6];commands.append(w(*state,*facts));trace=[]
 u.mem_write(b,bytes(0x8000));u.mem_write(0x5cb054,w(b if state[1]==1 else b+0x6000));u.mem_write(0x7c75d4,w(player(state[2],False)));u.mem_write(0x64ecb9,b'\0')
 u.mem_write(0x5a3ed8,w(state[3]));u.mem_write(0x62fd48,w(state[4]));u.mem_write(0x62fd44,w(state[5]))
 for i in (1,2):u.mem_write(player(i,False)+0xfb0,w(state[5+2*i]))
 before=bytes(u.mem_read(b,0x8000));u.mem_write(stack,w(b));u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x41fe5f,0x41feed,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x41feed
 result=[1,1 if get(u,0x5cb054)==b else 99,index(get(u,0x7c75d4),False),get(u,0x5a3ed8),get(u,0x62fd48),get(u,0x62fd44),1,get(u,player(1,False)+0xfb0),2,get(u,player(2,False)+0xfb0)]
 want=w(0,*result,len(trace)//2,*trace)+bytes((6-len(trace))*4);expected.append(want);stops+=int(2 in trace[::2])
 after=bytearray(u.mem_read(b,0x8000))
 for i in (1,2):off=player(i,False)+0xfb0-b;after[off]=before[off]
 assert after==before
 trace=[];x.mem_write(b,w(state[0],state[1],player(state[2],True),*state[3:6]));x.mem_write(b+0x1000,w(*state[6:]));x.mem_write(b+0x4000,w(b+0x5000,0));x.mem_write(stack,w(stop,b,b+0x4000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 result=[get(x,b),get(x,b+4),index(get(x,b+8),True),get(x,b+12),get(x,b+16),get(x,b+20),*struct.unpack('<4I',x.mem_read(b+0x1000,16))]
 got=w(x.reg_read(UC_X86_REG_EAX),*result,len(trace)//2,*trace)+bytes((6-len(trace))*4);assert got==want,('NXDK',case)
exe=root/'build/pc/Release/rf_entity_probe.exe';assert subprocess.check_output([str(exe),'--death-early'],input=b''.join(commands))==b''.join(expected),'PC'
report=dict(result='PASS',cases=len(commands),mode_stops=stops,original_sha256=digest,pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='SP41fe5f..41feed, original timers execute. Exact local identity/player rereads, single-byte fb0 clear, both global timers and callbacks, including callback pointer changes and wrap. Collision/mode backends supplied. No full live death dispatch or XEMU gameplay.')
(root/'artifacts/death-early.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Original SP activation bookkeeping with observable mutable dispatch."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
 m.mem_map(base,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_trigger_fire_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
from unicorn import UC_HOOK_CODE
u.mem_write(0x64ecb9,bytes(2));offsets=[0x2b0,0x2a0,0x298,0x29c,0x2a8,0x2c,0x2a4,0x7c]
trace=0;mutation=0;callback=base+0xc000
read=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def state(m,ptr,original):return [read(m,ptr+(offsets[i] if original else i*4)) for i in range(8)]
def dispatch(m,address,size,data):
 global trace
 original=address==0x4c0320;sp=m.reg_read(UC_X86_REG_ESP)
 if original:ptr,actor,suppress=struct.unpack('<3I',m.mem_read(sp+4,12))
 else:ptr,actor,suppress=struct.unpack('<3I',m.mem_read(sp+8,12))
 values=state(m,ptr,original);trace=2166136261
 for value in values+[actor,suppress]:trace=((trace^value)*16777619)&0xffffffff
 if mutation:
  values[0]^=8;values[1]=0xffffffff;values[6]=0;values[7]|=128
  for i in (0,1,6,7):m.mem_write(ptr+(offsets[i] if original else i*4),w(values[i]))
 ret=read(m,sp);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,dispatch,begin=0x4c0320,end=0x4c0320);x.hook_add(UC_HOOK_CODE,dispatch,begin=callback,end=callback)
rng=random.Random(0x4c0220);commands=bytearray();expected=bytearray();fired_count=0
for n in range(2048):
 flags=rng.choice([0,8,16,64,72,128]);count=rng.choice([0,1,3,0x7fffffff,0xffffffff]);deadline=rng.choice([0,100,0xffffffff]);cooldown=rng.choice([-1,0,1,1000]);limit=rng.choice([-2,-1,0,1,4]);objflags=rng.getrandbits(32)
 values=[flags,count,deadline,cooldown,0xbf800000,77,limit,objflags];now=rng.choice([0,100,1072799999,1072800000]);clock=0x42c80000;blocked=n%5==0;actor=123;suppress=rng.choice([0,1,2,256,257]);mutation=n%2
 command=w(*values,now,clock,blocked,actor,suppress,mutation);commands.extend(command)
 storage=bytearray(0x400)
 for i,v in enumerate(values):storage[offsets[i]:offsets[i]+4]=w(v)
 storage[0x2ac:0x2b0]=w(0xffffffff);u.mem_write(base,bytes(storage));u.mem_write(0x856844,w(blocked));u.mem_write(0x5a3ed8,w(now));u.mem_write(0x6460f0,w(clock));u.mem_write(stack,w(stop,base,actor,suppress,0));u.reg_write(UC_X86_REG_ESP,stack);trace=0
 u.emu_start(0x4c0220,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 fired=u.reg_read(UC_X86_REG_EAX)&255;assert fired==int(not blocked);fired_count+=fired
 want=w(*state(u,base,True),0,fired,trace);expected.extend(want)
 x.mem_write(base,command[:32]);x.mem_write(base+0x100,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base,now,clock,blocked,actor,suppress,callback,0,base+0x100));x.reg_write(UC_X86_REG_ESP,stack);trace=0
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=bytes(x.mem_read(base,32))+w(x.reg_read(UC_X86_REG_EAX),read(x,base+0x100),trace)
 assert got==want,('NXDK',n,got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-fire'],input=commands)
assert len(actual)==len(expected)
for n in range(2048):assert actual[n*44:(n+1)*44]==expected[n*44:(n+1)*44],('PC',n)
report=dict(result='PASS',cases=2048,fired=fired_count,original_sha256=digest,scope='Full SP 4c0220 with global inhibit and no player-field gate. Only linked dispatch intercepted; callback sees old state and mutates flags/count/limit/object flags in half fixtures. Original mark and timer helpers unchanged. Exact PC/NXDK final state, fired result and callback hash. Signed count/limit boundaries, count wrap, auto exemption, cooldown wrap. Player-field resolution, link effects, removal processing and live campaign excluded.')
(root/'artifacts/trigger-fire-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

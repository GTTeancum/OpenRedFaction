"""Full428a60 orchestration; clearance, player lookup and ground are explicit boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
base=0x30000000;cls=base+0x2000;records=base+0x4000;cache=base+0x5000;owner=base+0x6000
player=base+0x7000;flags=base+0x7100;position=base+0x7200;ops=base+0x8000;out=base+0x9000
callbacks=[base+0xa000,base+0xa100,base+0xa200];stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 pe=pefile.PE(str(path));data=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
 cpu=Uc(UC_ARCH_X86,UC_MODE_32);cpu.mem_map(origin,(len(data)+4095)//4096*4096);cpu.mem_write(origin,data);cpu.mem_map(base,65536);return cpu
u=load(exe);x=load(root/'build/xbox/main.exe')
mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_physics_try_stand\s+([0-9a-fA-F]+)',mapping)[1],16)
failure_stage=0
trace=[];endpoint=b'';blocked=has_player=lookup_xor=ground_xor=0

def hook(cpu,address,size,original):
 global endpoint
 stage=([0x499ed0,0x4a3740,0x4a0840] if original else callbacks).index(address)+1
 fp=base+0x810 if original else flags;pp=player+0xb1 if original else player
 trace.append([stage,r(cpu,fp),r(cpu,pp),r(cpu,records)])
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if stage==1:
  start_ptr=r(cpu,sp+(4 if original else 8));end_ptr=r(cpu,sp+(8 if original else 12))
  assert bytes(cpu.mem_read(start_ptr,12))==pos
  endpoint=bytes(cpu.mem_read(end_ptr,12))
  if original:ret=blocked
  else:cpu.mem_write(r(cpu,sp+16),w(blocked));ret=0
 elif stage==2:
  cpu.mem_write(fp,w(r(cpu,fp)^lookup_xor));ret=(player if original else pp) if has_player else 0
 else:
  cpu.mem_write(fp,w(r(cpu,fp)^ground_xor));ret=0
 if not original and stage==failure_stage:ret=0xfffffffe
 cpu.reg_write(UC_X86_REG_EAX,ret);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for a in [0x499ed0,0x4a3740,0x4a0840]:u.hook_add(UC_HOOK_CODE,hook,True,begin=a,end=a)
for a in callbacks:x.hook_add(UC_HOOK_CODE,hook,False,begin=a,end=a)
rng=random.Random(0x428a60);commands=[];expected=[]
for case in range(576):
 count=case%9;blocked=[0,1,256,257][case//9%4];has_player=case//36%2
 initial_flags=rng.getrandbits(32);initial_player=rng.getrandbits(32)
 lookup_xor=[0,0x400,0x80000][case//72%3];ground_xor=[0,0x400,0x100400][case//216%3]
 initial=rng.randbytes(192);target=f(*[rng.uniform(-2,2) for _ in range(24)])
 pos=f(*[rng.uniform(-100,100) for _ in range(3)]);height=f(rng.uniform(0,2))
 wire=w(blocked,has_player,initial_flags,initial_player,lookup_xor,ground_xor,count)+initial+target+pos+height
 u.mem_write(base,bytes(0x1500));u.mem_write(base+0x2c,w(0x12340005));u.mem_write(base+0x3c,pos)
 u.mem_write(base+0x184,w(count,8,records));u.mem_write(base+0x294,w(cls));u.mem_write(base+0x29c,w(cls));u.mem_write(base+0x810,w(initial_flags))
 u.mem_write(cls,bytes(0x2000));u.mem_write(cls+0xf74,height);u.mem_write(cls+0xcec,w(count)+b''.join(bytes(24)+target[i*12:i*12+12]+w(i) for i in range(8)))
 u.mem_write(records,initial);u.mem_write(player+0xb1,w(initial_player))
 trace=[];u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x428a60,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 stood=u.reg_read(UC_X86_REG_EAX)&255;after=bytes(u.mem_read(records,192));after_flags=r(u,base+0x810);after_player=r(u,player+0xb1)
 want_trace=list(trace);want_end=endpoint
 changed=bytearray(wire);changed[8:16]=w(after_flags,after_player);changed[28:220]=after
 tail=w(len(trace))+b''.join(w(*t) for t in trace)+bytes((3-len(trace))*16)+endpoint
 expected.append(w(0,stood)+changed+tail);commands.append(wire)
 x.mem_write(records,initial);x.mem_write(cache,w(count)+target+bytes(96)+height);x.mem_write(owner,w(records,count,192))
 x.mem_write(flags,w(initial_flags));x.mem_write(player,w(initial_player));x.mem_write(position,pos);x.mem_write(ops,w(*callbacks));x.mem_write(out,w(-99))
 trace=[];x.mem_write(stack,w(stop,owner,cache,position,flags,ops,0,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert x.reg_read(UC_X86_REG_EAX)==0 and r(x,out)==stood,(case,x.reg_read(UC_X86_REG_EAX),r(x,out),stood,trace,want_trace)
 assert bytes(x.mem_read(records,192))==after and r(x,flags)==after_flags and r(x,player)==after_player
 assert trace==want_trace and endpoint==want_end,(case,trace,want_trace)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--try-stand'],input=b''.join(commands))
assert len(actual)==len(expected)*404
for i,want in enumerate(expected):assert actual[i*404:(i+1)*404]==want,('PC',i)
# Explicit port failures: initial malformed arguments cannot query or mutate.
def reset_port():
 global trace,blocked,has_player,lookup_xor,ground_xor
 trace=[];blocked=0;has_player=1;lookup_xor=0x400;ground_xor=0x80000
 x.mem_write(records,initial);x.mem_write(cache,w(1)+target+bytes(96)+height)
 x.mem_write(owner,w(records,1,192));x.mem_write(flags,w(0x12340400));x.mem_write(player,w(0xabcdefff))
 x.mem_write(position,pos);x.mem_write(ops,w(*callbacks));x.mem_write(out,w(-99))
 return [owner,cache,position,flags,ops,0,out]
def run_port(args):
 x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 return x.reg_read(UC_X86_REG_EAX)
for case in range(10):
 args=reset_port()
 if case<5:args[case]=0
 elif case==5:args[6]=0
 elif case==6:x.mem_write(cache,w(0))
 elif case==7:x.mem_write(cache+4,w(0x7fc00000))
 elif case==8:x.mem_write(ops,w(0,*callbacks[1:]))
 else:x.mem_write(ops,w(*callbacks[:2],0))
 assert run_port(args)==0xfffffffc and trace==[],case
 assert bytes(x.mem_read(records,192))==initial and r(x,flags)==0x12340400 and r(x,player)==0xabcdefff and r(x,out)==0xffffff9d
for failure_stage in [1,3]:
 args=reset_port();assert run_port(args)==0xfffffffe and r(x,out)==0xffffff9d
 if failure_stage==1:
  assert len(trace)==1 and r(x,flags)==0x12340400 and r(x,player)==0xabcdefff and bytes(x.mem_read(records,192))==initial
 else:
  assert len(trace)==3 and r(x,flags)==((0x12340400&~0x400)^lookup_xor^ground_xor) and r(x,player)==0xabcdef00
  assert bytes(x.mem_read(records,12))==target[:12] and bytes(x.mem_read(records+12,180))==initial[12:]
failure_stage=0
report=dict(result='PASS',port_rejections=10,callback_errors=2,cases=len(expected),original_sha256=digest,scope='Full428a60 with actual center-copy and endpoint callees. Clearance499ed0, player lookup4a3740 and ground4a0840 are supplied boundaries. Exact PC/NXDK records, flags, player byte, endpoint, return and callback trace; lookup/ground flag mutations and low-byte blocked results. Geometry queries, cache construction, landing and player registry excluded.')
(root/'artifacts/try-stand-verification.json').write_text(json.dumps(report,indent=2));print(report)

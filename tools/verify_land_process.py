"""Original419830 landing orchestration, explicit audio/player-flag and stance boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_land_process\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
regions=[(0x810,0,4),(0x3724,4,4),(0x1a8,8,4),(0x520,12,4),(0x144,16,12),(0x8a0,28,12),(0x1d8,40,12),(0x3c,52,12),(0x1380,64,4),(0x3140,68,40)]
trace=[];wire=b'';fail_event=-1;failure_snapshot=None
word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
def emit(cpu,event,group,original):
 global failure_snapshot
 af,bf,action,vel,support,material=(0x810,0x1a8,0x520,0x144,0x8a0,0x1380) if original else (0,8,12,16,28,64)
 trace.append([event]+list(struct.unpack('<3I',bytes(cpu.mem_read(base+vel,12))))+[word(cpu,base+af),word(cpu,base+bf),group&0xffffffff,word(cpu,base+material)])
 mutation=struct.unpack_from('<I',wire,108)[0]
 if event==4 and mutation&1:
  for off,add in ((vel+8,1),(support+4,2)):
   v=struct.unpack('<f',bytes(cpu.mem_read(base+off,4)))[0];cpu.mem_write(base+off,floats(v+add))
  cpu.mem_write(base+action,pack(4))
 if event==3 and mutation&2:cpu.mem_write(base+af,pack(word(cpu,base+af)^0x400))
 if event<3 and mutation&4:cpu.mem_write(base+bf,pack(word(cpu,base+bf)^0x200000))
 if not original and event==fail_event:failure_snapshot=bytes(cpu.mem_read(base,108))
def ret(cpu):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
def original_hook(cpu,address,size,data):
 if address==0x4198b3:
  slot=cpu.reg_read(UC_X86_REG_EAX);emit(cpu,4,word(cpu,base+0x3140+slot*4),True);cpu.reg_write(UC_X86_REG_EIP,0x419901)
 elif address==0x419981:emit(cpu,3,-1,True);cpu.reg_write(UC_X86_REG_EIP,0x4199c5)
 elif address in (0x4280b0,0x428030):
  sp=cpu.reg_read(UC_X86_REG_ESP);assert word(cpu,sp+4)==base
  event=0 if address==0x4280b0 else 1+bool(word(cpu,sp+8)&255)
  emit(cpu,event,-1,True);ret(cpu)
def nx_hook(cpu,address,size,data):
 if address not in (base+0xf200,base+0xf210):return
 sp=cpu.reg_read(UC_X86_REG_ESP);assert word(cpu,sp+8)==base
 event=4 if address==base+0xf200 else word(cpu,sp+12);group=word(cpu,sp+12) if event==4 else -1
 emit(cpu,event,group,False);cpu.reg_write(UC_X86_REG_EAX,0xffffffff if event==fail_event else 0);ret(cpu)
u.hook_add(UC_HOOK_CODE,original_hook);x.hook_add(UC_HOOK_CODE,nx_hook)
x.mem_write(base+0x7000,pack(base+0xf200,base+0xf210,base+0x8000))
rng=random.Random(0x419830);commands=[];expected=[];counts=[0]*5;event_cases={}
for case in range(4096):
 w=bytearray(pack(rng.choice((0,0x400,0x1000,0x100400)),rng.choice((0,0x2000000)),rng.getrandbits(32),rng.choice((0,4,0xffffffff))))
 w+=floats(*[rng.randrange(-16,17)/16 for _ in range(12)])+pack(rng.choice((-1,0,1,2,3,4,9)))+pack(*[rng.choice((-1,0,2,3)) for _ in range(10)])+pack(rng.randrange(8))
 if case%3==0:w[16:28]=floats(rng.choice((0,.5,.5000000596046448)),0,0);w[40:52]=floats(0,rng.choice((0,.25,.2500000298023224)),0)
 wire=bytes(w);assert len(wire)==112
 u.mem_write(base,bytes(0x5000))
 for off,src,n in regions:u.mem_write(base+off,wire[src:src+n])
 u.mem_write(base+0x294,pack(base+0x3000));u.mem_write(stack,pack(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];u.emu_start(0x419830,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop,('original',case)
 out=bytearray(wire[:108])
 for off,dst,n in regions:out[dst:dst+n]=bytes(u.mem_read(base+off,n))
 original_trace=trace[:];result=bytes(out)+pack(len(trace))+b''.join(pack(*r) for r in trace)+bytes((3-len(trace))*32)
 for e in trace:counts[e[0]]+=1;event_cases.setdefault(e[0],(wire,original_trace))
 x.mem_write(base,wire);x.mem_write(stack,pack(stop,base,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,('NXDK return',case)
 got=bytes(x.mem_read(base,108))+pack(len(trace))+b''.join(pack(*r) for r in trace)+bytes((3-len(trace))*32)
 assert got==result,('NXDK',case,trace,original_trace)
 commands.append(wire);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--land-process'],input=b''.join(commands));assert pc==b''.join(expected),'PC mismatch'
for fail_event,(wire,normal_trace) in event_cases.items():
 x.mem_write(base,wire);x.mem_write(stack,pack(stop,base,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];failure_snapshot=None;x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xffffffff and failure_snapshot==bytes(x.mem_read(base,108))
 end=next(i for i,r in enumerate(normal_trace) if r[0]==fail_event)+1;assert trace==normal_trace[:end]
fail_event=-1;invalid_cases=0
for off in range(16,52,4):
 w=bytearray(commands[0]);w[off:off+4]=pack(0x7fc12345);wire=bytes(w)
 x.mem_write(base,wire);x.mem_write(stack,pack(stop,base,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffe and not trace and bytes(x.mem_read(base,108))==wire[:108]
 invalid_cases+=1
w=bytearray(commands[0]);w[16:28]=floats(1,0,0);w[64:68]=pack(10);wire=bytes(w)
x.mem_write(base,wire);x.mem_write(stack,pack(stop,base,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];x.emu_start(entry,stop,count=100000)
assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and not trace and bytes(x.mem_read(base,108))==wire[:108]
invalid_cases+=1
report=dict(result='PASS',cases=len(commands),invalid_cases=invalid_cases,callback_failures=len(event_cases),callback_counts=counts,original_sha256=sha,scope='Original419830 through return with real speed/relative-Y sound gate, material/group selection, vector helpers and stance predicate. Sound/player-flag block4198b3..419901, special block419981..4199c5 and4280b0/428030 supplied. Exact PC/NXDK retained state and ordered callback arguments, threshold boundaries, callback velocity/support/action/flag mutations and post-stance flag clearing. Callback internals, nonfinite velocity domain and scene scheduling excluded.')
(root/'artifacts/land-process-verification.json').write_text(json.dumps(report,indent=2));print(report)

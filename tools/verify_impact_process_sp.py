"""Original49cd80 SP ordering vs PC/NXDK, with explicit effect boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_impact_process_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
regions=[(0x2c,0,4),(0x7c,4,4),(0x4004,8,4),(0x31b4,12,4),(0x1d0,16,4),(0x1380,20,4),(0x3128,24,4),(0x34,28,4),(0x3c,32,12),(0xe4,44,12)]
trace=[];wire=b'';fail_event=-1;failure_snapshot=None
word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
def row(event,args):trace.append([event]+args+[0]*(7-len(args)))
def ret(cpu):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
def mutate(cpu,e,original):
 mutation=struct.unpack_from('<I',wire,68)[0]
 handle,health,sound,y=(0x2c,0x34,0x3128,0xe8) if original else (0,28,24,48)
 if e==0 and mutation&1:cpu.mem_write(base+handle,pack(word(cpu,base+handle)^0x10000))
 if e==1:
  cpu.mem_write(base+health,wire[64:68])
  if mutation&2:
   cpu.mem_write(base+sound,pack(word(cpu,base+sound)^0x55))
   value=struct.unpack('<f',bytes(cpu.mem_read(base+y,4)))[0];cpu.mem_write(base+y,floats(value+2))
def original_hook(cpu,address,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x45ce50:
  assert word(cpu,sp+4)==base+0x3c;row(0,list(struct.unpack('<3I',bytes(cpu.mem_read(base+0x3c,12)))))
  mutate(cpu,0,True);cpu.reg_write(UC_X86_REG_EAX,struct.unpack_from('<I',wire,60)[0]);ret(cpu)
 elif address==0x4892c0:
  a=[word(cpu,sp+4+i*4) for i in range(8)];assert a[3]==0xffffffff;row(1,a[:3]+a[4:]);mutate(cpu,1,True);cpu.reg_write(UC_X86_REG_EIP,base+0xf100)
 elif address==0x49ce88:
  row(2,[word(cpu,base+0x2c),word(cpu,base+0x3128)]+list(struct.unpack('<3I',bytes(cpu.mem_read(base+0xe4,12)))))
  cpu.reg_write(UC_X86_REG_EIP,0x49cf34)
 elif address==0x49ced0:
  row(3,[word(cpu,base+0x2c),word(cpu,sp+0x18)]);cpu.reg_write(UC_X86_REG_EIP,0x49cf34)
def nx_hook(cpu,address,size,data):
 global failure_snapshot
 if not base+0xf200<=address<base+0xf240 or (address-base-0xf200)%16:return
 e=(address-base-0xf200)//16;sp=cpu.reg_read(UC_X86_REG_ESP);a=[word(cpu,sp+4+i*4) for i in range(3)];assert a[1]==base
 if e==0:row(e,list(struct.unpack('<3I',bytes(cpu.mem_read(base+32,12)))));cpu.mem_write(a[2],wire[60:64])
 if e==1:row(e,[word(cpu,base)]+list(struct.unpack('<6I',bytes(cpu.mem_read(a[2],24)))))
 if e==2:row(e,[word(cpu,base),word(cpu,base+24)]+list(struct.unpack('<3I',bytes(cpu.mem_read(base+44,12)))))
 if e==3:row(e,[word(cpu,base),a[2]])
 mutate(cpu,e,False)
 if e==fail_event:failure_snapshot=bytes(cpu.mem_read(base,56))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if e==fail_event else 0);ret(cpu)
u.hook_add(UC_HOOK_CODE,original_hook);x.hook_add(UC_HOOK_CODE,nx_hook);u.mem_write(base+0xf100,b'\xd9\xee\xc3')
x.mem_write(base+0x7000,pack(*[base+0xf200+i*16 for i in range(4)],base+0x8000))
rng=random.Random(0x49cd80);commands=[];expected=[];counts=[0]*4;event_cases={}
for case in range(4096):
 w=bytearray(pack(0x50002,rng.choice((0,0,4,8)),rng.choice((1,3,8)),rng.choice((0,1,2)),rng.choice((0,3,0xffffffff)),rng.choice((0,0xffffffff)),rng.randrange(20))+floats(100,*[rng.randrange(-32,33)/4 for _ in range(6)]))
 w+=floats(rng.choice((0,7,9,10,11,20,100)))+pack(rng.choice((0,0,1,256,257)))+floats(rng.choice((-10,0,10)))+pack(rng.randrange(4))
 if case%31==0:w[64:68]=pack(0x7fc12345)
 wire=bytes(w);assert len(wire)==72
 u.mem_write(base,bytes(0x5000))
 for off,src,n in regions:u.mem_write(base+off,wire[src:src+n])
 u.mem_write(base+0x294,pack(base+0x3000));u.mem_write(base+0x858,pack(base+0x4000));u.mem_write(0x64ecb9,b'\0')
 u.mem_write(stack,pack(stop,base)+wire[56:60]);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];u.emu_start(0x49cd80,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop,('original',case)
 out=bytearray(wire[:56])
 for off,dst,n in regions:out[dst:dst+n]=bytes(u.mem_read(base+off,n))
 original_trace=trace[:];result=bytes(out)+pack(len(trace))+b''.join(pack(*r) for r in trace)+bytes((3-len(trace))*32)
 for e in trace:counts[e[0]]+=1;event_cases.setdefault(e[0],(wire,original_trace))
 x.mem_write(base,wire);x.mem_write(stack,pack(stop,base)+wire[56:60]+pack(base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,('NXDK return',case)
 got=bytes(x.mem_read(base,56))+pack(len(trace))+b''.join(pack(*r) for r in trace)+bytes((3-len(trace))*32)
 assert got==result,('NXDK',case,trace,original_trace)
 commands.append(wire);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--impact-process-sp'],input=b''.join(commands));assert pc==b''.join(expected),'PC mismatch'
for fail_event,(wire,normal_trace) in event_cases.items():
 x.mem_write(base,wire);x.mem_write(stack,pack(stop,base)+wire[56:60]+pack(base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];failure_snapshot=None;x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xffffffff and failure_snapshot==bytes(x.mem_read(base,56))
 end=next(i for i,r in enumerate(normal_trace) if r[0]==fail_event)+1;assert trace==normal_trace[:end]
fail_event=-1;invalid_cases=0
for speed_bits in (0x7fc12345,0x7f800000,0xff800000,0x7f7fffff):
 wire=commands[0][:56]+pack(speed_bits)+commands[0][60:]
 x.mem_write(base,wire);x.mem_write(stack,pack(stop,base)+wire[56:60]+pack(base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and not trace and bytes(x.mem_read(base,56))==wire[:56]
 invalid_cases+=1
report=dict(result='PASS',cases=len(commands),invalid_cases=invalid_cases,callback_failures=len(event_cases),callback_counts=counts,original_sha256=sha,scope='Original49cd80 SP entry through return, real falling/kind predicates and damage arithmetic. Force suppression45ce50, damage4892c0, lethal sound block49ce88..49cecf, and player lookup/feedback block49ced0..49cf31 supplied as explicit effect boundaries. Exact PC/NXDK actor state and callback arguments/order, low-byte suppression, material-sensitive falling, post-damage health including unordered, and reentrant handle/sound/position changes. Callback internals and scene scheduling excluded.')
(root/'artifacts/impact-process-sp-verification.json').write_text(json.dumps(report,indent=2));print(report)

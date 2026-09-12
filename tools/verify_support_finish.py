"""Complete original49d7e0 SP orchestration with explicit gameplay callback boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EBX
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_physics_support_finish\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)


regions=[(0x88,0,12),(0x98,12,236),(0x190,248,32),(0x1c0,280,16),(0x1e4,296,4),(0x1ec,300,8),(0x1b4,308,12),(0x1d0,320,20),(0x1e8,340,4),(0x1f4,344,4),(0x8ac,348,4),(0x1380,352,4),(0x3c,356,12),(0x1474,376,4)]
trace=[];wire=b'';mutation=0;fail_event=-1;failure_snapshot=None;event_cases={}
word=lambda cpu,a:struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
def row(e,args=[]):trace.append([e]+args+[0]*(7-len(args)))
def mutate(cpu,e,original):
 mode=base+(0x4004 if original else 0x6010);flags=base+(0x1a8 if original else 272);material=base+(0x1380 if original else 352)
 vy=base+(0x148 if original else 188);vz=base+(0x14c if original else 192);field=base+(0x1f0 if original else 304);pub=base+(0x3c if original else 356);rel=base+(0x1474 if original else 0x6018)
 if e==1 and mutation&1:cpu.mem_write(mode,pack(3));cpu.mem_write(flags,pack(word(cpu,flags)|1));cpu.mem_write(material,pack(0xffffffff))
 if e==2 and mutation&2:cpu.mem_write(mode,pack(1));cpu.mem_write(vy,floats([.5]));cpu.mem_write(field,pack(word(cpu,field)^0x12345))
 if e==3 and mutation&4:
  cpu.mem_write(mode,pack(1));cpu.mem_write(vz,pack(0));v=struct.unpack('<f',bytes(cpu.mem_read(pub,4)))[0];cpu.mem_write(pub,floats([v+2]));cpu.mem_write(rel,pack(word(cpu,rel)^255))
def ret(cpu,extra=0):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,word(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+extra)
addresses={0x40a0e0:0,0x4281a0:1,0x49cd80:2,0x419830:3,0x4e5c60:4}
def original_hook(cpu,address,size,data):
 if address not in addresses:return
 e=addresses[address];sp=cpu.reg_read(UC_X86_REG_ESP);a=[word(cpu,sp+4+i*4) for i in range(3)]
 if e==0:row(e,[a[0]]);return
 if e==1:assert a[0]==base;row(e)
 if e==2:assert a[0]==base;row(e,[a[1]])
 if e==3:assert a[0]==base and a[1]==base+0xf0;row(e,list(struct.unpack('<3I',bytes(cpu.mem_read(base+0xf0,12)))))
 if e==4:
  h=word(cpu,a[2]+48);time=word(cpu,a[2]+24);row(e,[a[1],h,time]);cpu.mem_write(a[0],pack(a[1]^h^0x55aa55aa));cpu.reg_write(UC_X86_REG_EAX,a[0])
 mutate(cpu,e,True);ret(cpu,12 if e==4 else 0)
def nx_hook(cpu,address,size,data):
 global failure_snapshot
 if address<base+0xf200 or address>=base+0xf250 or (address-base-0xf200)%16:return
 e=(address-base-0xf200)//16;sp=cpu.reg_read(UC_X86_REG_ESP);a=[word(cpu,sp+4+i*4) for i in range(4)]
 if e==0:row(e,[a[1]]);cpu.mem_write(a[2],wire[532:548])
 if e==1:row(e)
 if e==2:row(e,[a[2]])
 if e==3:row(e,list(struct.unpack('<3I',bytes(cpu.mem_read(base+100,12)))))
 if e==4:
  h=word(cpu,a[2]+48);time=word(cpu,a[2]+24);row(e,[a[1],h,time]);cpu.mem_write(a[3],pack(a[1]^h^0x55aa55aa))
 mutate(cpu,e,False)
 if e==fail_event:failure_snapshot=bytes(cpu.mem_read(base,368))+bytes(cpu.mem_read(base+0x6010,12))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if e==fail_event else 0);ret(cpu)
u.hook_add(UC_HOOK_CODE,original_hook);x.hook_add(UC_HOOK_CODE,nx_hook)
x.mem_write(base+0x7000,pack(*[base+0xf200+i*16 for i in range(5)],base+0x8000))
rng=random.Random(0x4a0c05);commands=[];expected=[];counts=[0]*5
for case in range(4096):
 w=bytearray(rng.getrandbits(8) for _ in range(552))
 for off,n in ((88,6),(184,3),(356,3),(380,6),(464,6),(500,3)):w[off:off+n*4]=floats([rng.randrange(-2048,2049)/32 for _ in range(n)])
 w[244:248]=floats([rng.choice((.5,1,3))]);w[352:356]=pack(rng.choice((0,0xffffffff)));w[368:376]=pack(rng.choice((1,3,8)),rng.choice((0,1)))
 w[476:488]=floats([rng.randrange(-16,17)/16,rng.choice((-.5,0,.49999997,.5,1)),rng.randrange(-16,17)/16])
 w[488:492]=floats([rng.choice((0,.25,.999,1,2))]);w[492:496]=pack(rng.choice((0,3,0xffffffff)))
 present=case%3==0;h=(0x80010002 if case%2 else 0x10002) if present else (0xffffffff if case%2 else 0x20002)
 w[512:516]=pack(h);w[524:528]=pack(0x5555 if case%4 else 0)
 w[532:548]=pack(int(present),rng.choice((0,3)),rng.choice((0,0x80000000)),h if present else 0)
 w[548:552]=pack(case%8);wire=bytes(w);mutation=case%8
 actor=bytearray(0x1500)
 for off,src,n in regions:actor[off:off+n]=wire[src:src+n]
 actor[0x24:0x28]=pack(0);actor[0x294:0x298]=pack(base+0x3000);actor[0x858:0x85c]=pack(base+0x4000)
 u.mem_write(base,bytes(actor));u.mem_write(base+0x31b4,wire[372:376]);u.mem_write(base+0x4004,wire[368:372]);u.mem_write(base+0x2000,bytes(0x200));u.mem_write(base+0x2024,wire[536:540]);u.mem_write(base+0x202c,pack(h if present else 0x10002));u.mem_write(base+0x21a8,wire[540:544]);u.mem_write(0x7394d4,pack(base+0x2000));u.mem_write(0x7c6ec8,floats([0,1,0]))
 u.mem_write(stack,bytes(0x100));u.mem_write(stack+0x10,wire[384:388]);u.mem_write(stack+0x1c,wire[396:400]);u.mem_write(stack+0x48,wire[464:532]);u.mem_write(stack+0x9c,pack(stop));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,base+0xf0);u.reg_write(UC_X86_REG_ESP,stack-16);u.reg_write(UC_X86_REG_FPCW,0x27f);trace=[]
 u.emu_start(0x4a0a5c,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop,('original',case)
 out=bytearray(wire[:380])
 for off,dst,n in regions:out[dst:dst+n]=bytes(u.mem_read(base+off,n))
 out[368:372]=bytes(u.mem_read(base+0x4004,4));out[372:376]=bytes(u.mem_read(base+0x31b4,4))
 original_trace=trace[:];result=bytes(out)+pack(len(trace))+b''.join(pack(*r) for r in trace)+bytes((5-len(trace))*32)
 for rowdata in trace:
  counts[rowdata[0]]+=1;event_cases.setdefault(rowdata[0],(wire,original_trace))
 x.mem_write(base,wire);x.mem_write(base+0x6000,pack(base,base+308,base+348,base+356)+wire[368:380]);x.mem_write(stack,pack(stop,base+0x6000,base+380,base+464,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[]
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,('return',case)
 got=bytes(x.mem_read(base,368))+bytes(x.mem_read(base+0x6010,12))+pack(len(trace))+b''.join(pack(*r) for r in trace)+bytes((5-len(trace))*32)
 assert got==result,('NXDK',case,trace,original_trace,[(i,got[i:i+4].hex(),result[i:i+4].hex()) for i in range(0,len(got),4) if got[i:i+4]!=result[i:i+4]][:12])
 assert bytes(x.mem_read(base+380,172))==wire[380:]
 commands.append(wire);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--support-finish'],input=b''.join(commands));assert pc==b''.join(expected),'PC mismatch'
failure_cases=0
for fail_event,(wire,normal_trace) in sorted(event_cases.items()):
 mutation=struct.unpack_from('<I',wire,548)[0]
 x.mem_write(base,wire);x.mem_write(base+0x6000,pack(base,base+308,base+348,base+356)+wire[368:380]);x.mem_write(stack,pack(stop,base+0x6000,base+380,base+464,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];failure_snapshot=None
 x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xffffffff
 end=next(i for i,row in enumerate(normal_trace) if row[0]==fail_event)+1
 assert trace==normal_trace[:end] and failure_snapshot==bytes(x.mem_read(base,368))+bytes(x.mem_read(base+0x6010,12))
 failure_cases+=1
report=dict(result='PASS',cases=len(commands),callback_failures=failure_cases,callback_counts=counts,original_sha256=sha,scope='Original4a0a5c through return with real lookup, predicates, contact copy and numeric support/bounds helpers. Fall/impact/land/relative callbacks supplied. Exact PC/NXDK retained body/contact/support/published/actor state and ordered callback arguments. Initial/final material predicates, static/moving/rejected support, high-bit relative tokens and reentrant impact/landing changes. Ground query and callback internals excluded.')
(root/'artifacts/support-finish-verification.json').write_text(json.dumps(report,indent=2));print(report)

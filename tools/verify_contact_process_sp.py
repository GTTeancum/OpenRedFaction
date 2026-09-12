"""Complete original49d7e0 SP orchestration with explicit gameplay callback boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ECX
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_physics_contact_process_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)

regions=[(0x88,0,12),(0x98,12,236),(0x190,248,32),(0x1c0,280,16),(0x1e4,296,4),(0x1ec,300,8),
 (0x1b4,308,12),(0x1d0,320,20),(0x1e8,340,4),(0x1f4,344,4),(0x8a0,348,12),(0x714,360,12),
 (0x2c,372,4),(0x810,384,4),(0x7c,388,4),(0x184,392,4),(0x964,396,4),(0x974,400,4),(0x1380,404,4)]
original_fields={'flags':0x1a8,'aflags':0x810,'mode':0x4004,'vy':0x148,'nz':0x1c4,'inv':0x1d4,'support':0x8a0,'direction':0x714,'kind':0x31b4,'count':0x184,'material':0x1380,'objectflags':0x7c,'vz':0x14c,'cy':0x1dc,'word':0x1f0,'velocity':0x144}
xfields={'flags':272,'aflags':0x6000+44,'mode':0x6000+36,'vy':188,'nz':284,'inv':324,'support':0x6000+8,'direction':0x6000+20,'kind':0x6000+40,'count':0x6000+52,'material':0x6000+64,'objectflags':0x6000+48,'vz':192,'cy':332,'word':304,'velocity':184}
trace=[];mutation=player_present=0;wire=b'';fail_event=-1;failure_snapshot=None;event_cases={}
def word(cpu,a):return struct.unpack('<I',bytes(cpu.mem_read(a,4)))[0]
def mutate(cpu,event,original):
 f=original_fields if original else xfields
 def put(k,v):cpu.mem_write(base+f[k],v)
 if event==1 and mutation&1:
  put('flags',pack(word(cpu,base+f['flags'])^0x80));put('aflags',pack(word(cpu,base+f['aflags'])|0x400));put('mode',pack(3))
  vy=struct.unpack('<f',bytes(cpu.mem_read(base+f['vy'],4)))[0];put('vy',floats([vy-1]));put('nz',floats([-.25]));put('inv',floats([.5]))
 if event==2 and mutation&2:
  for k,v in [('support',floats([2])),('direction',floats([1])),('kind',pack(1)),('count',pack(2)),('material',pack(0xffffffff))]:put(k,v)
 if event==3 and mutation&4:
  put('mode',pack(1));put('objectflags',pack(word(cpu,base+f['objectflags'])^8));put('vz',floats([5]));put('cy',floats([3]))
 if (event==5 and mutation&16) or (event==6 and mutation&8):put('word',pack(word(cpu,base+f['word'])^0x12345))
 if event==5 and mutation&16:put('velocity',floats([17,18,19]))
def row(event,args):trace.append([event]+args+[0]*(7-len(args)))
def return_from(cpu,extra=0):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=word(cpu,sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+extra);cpu.reg_write(UC_X86_REG_EIP,ret)
addresses={0x40a0e0:0,0x4289d0:1,0x427450:2,0x42a580:3,0x4a3740:4,0x4892c0:5,0x49cd80:6}
def original_hook(cpu,address,size,data):
 if address not in addresses:return
 e=addresses[address];sp=cpu.reg_read(UC_X86_REG_ESP);a=[word(cpu,sp+4+i*4) for i in range(8)];handle=word(cpu,base+0x2c)
 if e==0:row(0,[a[0]]);return # real lookup runs
 if e==1:assert a[0]==base;row(e,[handle])
 if e==2:assert cpu.reg_read(UC_X86_REG_ECX)==base;row(e,[handle,a[0]])
 if e==3:assert a[0]==base;row(e,[handle,a[1],a[2]])
 if e==4:row(e,[a[0]]);cpu.reg_write(UC_X86_REG_EAX,base+0x5000 if player_present else 0)
 if e==5:assert a[4]==0;row(e,a[:4]+a[5:8])
 if e==6:assert a[0]==base;row(e,[handle,a[1]])
 mutate(cpu,e,True)
 if e==5:cpu.reg_write(UC_X86_REG_EIP,base+0xf100) # fldz;ret matches ignored float damage return
 else:return_from(cpu,4 if e==2 else 0)
def nx_hook(cpu,address,size,data):
 global failure_snapshot
 if address<base+0xf200 or address>=base+0xf270 or (address-(base+0xf200))%16:return
 e=(address-(base+0xf200))//16;sp=cpu.reg_read(UC_X86_REG_ESP);a=[word(cpu,sp+4+i*4) for i in range(4)]
 handle=word(cpu,base+0x6020)
 if e==0:row(e,[a[1]]);cpu.mem_write(a[2],wire[408:420])
 if e==1:row(e,[handle])
 if e==2:row(e,[handle,a[2]])
 if e==3:row(e,[handle,a[2],a[3]])
 if e==4:row(e,[a[1]]);cpu.mem_write(a[2],pack(base+0x50b1 if player_present else 0))
 if e==5:row(e,[handle]+list(struct.unpack('<6I',bytes(cpu.mem_read(a[2],24)))))
 if e==6:row(e,[handle,a[2]])
 mutate(cpu,e,False)
 if e==fail_event:failure_snapshot=bytes(cpu.mem_read(base,348))+bytes(cpu.mem_read(base+0x6008,60))+bytes(cpu.mem_read(base+0x50b1,1))
 cpu.reg_write(UC_X86_REG_EAX,0xffffffff if e==fail_event else 0);return_from(cpu)
u.hook_add(UC_HOOK_CODE,original_hook);x.hook_add(UC_HOOK_CODE,nx_hook);u.mem_write(base+0xf100,b'\xd9\xee\xc3')
x.mem_write(base+0x7000,pack(*[base+0xf200+16*i for i in range(7)],base+0x8000))
rng=random.Random(0x49d7e0);commands=[];expected=[];route_counts=[0]*7
for case in range(2048):
 w=bytearray(rng.getrandbits(8) for _ in range(428))
 for off,n in ((88,3),(112,9),(184,6),(280,3),(308,3),(328,3),(348,6)):w[off:off+n*4]=floats([rng.randrange(-32,33)/16 for _ in range(n)])
 if case%4==0:w[112:148]=floats([1,0,0,0,1,0,0,0,1])
 w[300:304]=pack(1 if case%13==0 else 0);w[324:328]=floats([.25 if case%3==0 else 0])
 w[372:408]=pack(0x50002,rng.choice((1,3,8)),rng.choice((0,1)),rng.choice((0,0x400)),rng.choice((0,8)),rng.choice((0,1,2,3)),rng.choice((0,0xffffffff)),0xffffffff,rng.choice((0,0xffffffff)))
 w[408:420]=pack(int(case%5!=0),rng.choice((0,8)))+floats([2])
 w[420:428]=pack(rng.randrange(32),case%2)
 if case%8 in (0,1):
  w[300:304]=pack(0);w[324:328]=pack(0);w[376:380]=pack(1);w[408:412]=pack(1);w[284:288]=floats([-1]);w[328:340]=floats([0,-2,0]);w[348:360]=floats([0,0,0])
 w[296:300]=pack(0x10000 if struct.unpack_from('<I',w,408)[0] else 0x20000)
 wire=bytes(w);mutation,player_present=struct.unpack_from('<2I',wire,420)
 actor=bytearray(0x1500)
 for off,src,n in regions:actor[off:off+n]=wire[src:src+n]
 actor[0x24:0x28]=pack(0);actor[0x294:0x298]=pack(base+0x3000);actor[0x858:0x85c]=pack(base+0x4000)
 u.mem_write(base,bytes(actor));u.mem_write(base+0x31b4,wire[380:384]);u.mem_write(base+0x4004,wire[376:380]);u.mem_write(base+0x2000,bytes(0x200));u.mem_write(base+0x202c,pack(0x10000));u.mem_write(base+0x207c,wire[412:416]);u.mem_write(base+0x2180,wire[416:420]);u.mem_write(0x7394cc,pack(base+0x2000));u.mem_write(0x64ecb9,b'\0');u.mem_write(base+0x50b1,b'\x07')
 u.mem_write(stack,pack(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];u.emu_start(0x49d7e0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop,('original',case)
 out=bytearray(wire[:408])
 for off,dst,n in regions:out[dst:dst+n]=bytes(u.mem_read(base+off,n))
 out[376:380]=bytes(u.mem_read(base+0x4004,4));out[380:384]=bytes(u.mem_read(base+0x31b4,4))
 original_trace=trace[:];flag=u.mem_read(base+0x50b1,1)[0]
 result=bytes(out)+pack(flag,len(trace))+b''.join(pack(*r) for r in trace)+bytes((8-len(trace))*32)
 for e in trace:
  route_counts[e[0]]+=1;event_cases.setdefault(e[0],(wire,original_trace))
 x.mem_write(base,wire);x.mem_write(base+0x6000,pack(base,base+308)+wire[348:408]);x.mem_write(base+0x50b1,b'\x07')
 x.mem_write(stack,pack(stop,base+0x6000,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,('NXDK return',case)
 got=bytes(x.mem_read(base,348))+bytes(x.mem_read(base+0x6008,60))+pack(x.mem_read(base+0x50b1,1)[0],len(trace))+b''.join(pack(*r) for r in trace)+bytes((8-len(trace))*32)
 assert got==result,('NXDK',case,trace,original_trace,[(i,got[i:i+4].hex(),result[i:i+4].hex()) for i in range(0,len(got),4) if got[i:i+4]!=result[i:i+4]][:10])
 commands.append(wire);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--contact-process-sp'],input=b''.join(commands))
assert pc==b''.join(expected),('PC mismatch',next((i for i,(a,b) in enumerate(zip(pc,b''.join(expected))) if a!=b),None))
failure_cases=0
for fail_event,(wire,normal_trace) in sorted(event_cases.items()):
 mutation,player_present=struct.unpack_from('<2I',wire,420)
 x.mem_write(base,wire);x.mem_write(base+0x6000,pack(base,base+308)+wire[348:408]);x.mem_write(base+0x50b1,b'\x07')
 x.mem_write(stack,pack(stop,base+0x6000,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);trace=[];failure_snapshot=None
 x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xffffffff
 end=next(i for i,row in enumerate(normal_trace) if row[0]==fail_event)+1
 assert trace==normal_trace[:end]
 assert failure_snapshot==bytes(x.mem_read(base,348))+bytes(x.mem_read(base+0x6008,60))+bytes(x.mem_read(base+0x50b1,1))
 failure_cases+=1
report=dict(result='PASS',cases=len(commands),callback_failures=failure_cases,callback_counts=route_counts,original_sha256=sha,scope='Complete original49d7e0 SP execution against PC/NXDK orchestration. Actual object lookup, predicates and numeric helpers; only crouch/speed/motion/player lookup/crush damage/impact callbacks supplied. Exact body/contact/actor facts, player flag and ordered callback arguments. Reentrant stance mutations change flags, mode, class, support and contact values; crush callback velocity is cleared afterward. Callback internals and live scene ownership excluded.')
(root/'artifacts/contact-process-sp-verification.json').write_text(json.dumps(report,indent=2));print(report)

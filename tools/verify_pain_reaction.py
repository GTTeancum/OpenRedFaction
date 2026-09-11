"""Original428740 orchestration vs PC and NXDK with observable owner callbacks."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
b=0x30000000;stack=b+0xe000;stop=b+0xf000;stub=b+0xf100
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(im)+4095)//4096*4096);m.mem_write(origin,im);m.mem_map(b,65536);m.mem_write(stub,b'\xdd\x05'+w(stub+16)+b'\xc3');return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_pain_react\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
queries={0x42a8e0:0,0x40d740:1,0x4fa3f0:2,0x427020:3,0x408e90:4,0x408ef0:5,0x408ec0:6,0x41f950:9}
effects={0x41ae70:10,0x428c90:11,0x4fa3b0:12,0x4fa360:13}
facts=[];trace=[];mutation=0

def mutate(m,original):
 if original:
  m.mem_write(b+0x828,w(24));m.mem_write(b+0x80,w(77));m.mem_write(b+0xa54+22*16,w(88));m.mem_write(b+0xa54+23*16,w(99))
 else:
  m.mem_write(b+12,w(24));m.mem_write(b+8,w(77));m.mem_write(b+16+22*4,w(88));m.mem_write(b+16+23*4,w(99))
def hook(m,address,size,ctx):
 original=m is u
 if original and address not in queries and address not in effects and address not in (0x428d10,0x5033e0):return
 if not original and address not in (b+0x3000,b+0x3010,b+0x3020):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<7I',m.mem_read(sp,28));cleanup=0;value=0
 if original:
  if address in queries or address==0x428d10:
   kind=queries.get(address,7 if a[2]==2 else 8);row=(kind,0,0);value=facts[kind]
  elif address==0x5033e0:row=(14,a[1],a[2])
  else:
   kind=effects[address]
   row=(kind,a[1],a[2]) if kind==10 or kind==12 else (kind,a[2],0) if kind==11 else (kind,a[1],0)
   if kind==11:assert a[3:6]==(0x3f800000,0,1)
   cleanup=8 if kind==12 else 4 if kind==13 else 0
 else:
  if address==b+0x3000:kind=a[2];row=(kind,0,0);value=facts[kind]
  elif address==b+0x3010:row=(10+a[2],a[3],a[4])
  else:row=(14,a[2],a[3])
 trace.append(row)
 if row[0] in range(10,14) and mutation==row[0]-9:mutate(m,original)
 if row[0]==14:m.reg_write(UC_X86_REG_EIP,stub);return
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4+cleanup);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x428740);inputs=[];expected=[];accepted=0
for case in range(2048):
 facts=[rng.choice([0,1,2,255,256,257]) for _ in range(10)]
 if case%2==0:facts=[0,0,1,0,1,0,0,0,0,case%4//2]
 action=rng.choice([-1,-1,22,23,24]);fire=rng.choice([-1,3]);motions=[i+100 for i in range(45)]
 if case%5==0:motions[22]=motions[23]=motions[24]=-1
 motions[28]=fire
 state=[0x12340001,[-1,0,1,63,64,-2147483648,0x76540001][case%7],42,action]+motions
 seconds=rng.choice([0,.0005,.0015,.333333333333,1,1.2345,2.9994999999999,-.1]);mutation=case%5
 wire=w(*state,*facts)+struct.pack('<d',seconds)+w(mutation);inputs.append(wire)
 u.mem_write(b,bytes(0x1500));u.mem_write(b+0x2c,w(state[0]));u.mem_write(b+0x2a4,w(state[1]));u.mem_write(b+0x80,w(state[2]));u.mem_write(b+0x828,w(action));u.mem_write(b+0xc14,w(fire));u.mem_write(b+0x1430,w(b+0x6000))
 for i,v in enumerate(motions):u.mem_write(b+0xa54+i*16,w(v))
 u.mem_write(stub+16,struct.pack('<d',seconds));u.mem_write(stack,w(stop,b));trace=[]
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x428740,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 wantstate=w(state[0],state[1])+bytes(u.mem_read(b+0x80,4))+bytes(u.mem_read(b+0x828,4))+b''.join(bytes(u.mem_read(b+0xa54+i*16,4)) for i in range(45))
 want=w(0)+wantstate+w(len(trace))+b''.join(w(*row) for row in trace)+bytes((16-len(trace))*12);expected.append(want)
 accepted+=any(row[0]==11 for row in trace)
 x.mem_write(b,w(*state));x.mem_write(b+0x2000,w(b+0x3000,b+0x3010,b+0x3020,0));x.mem_write(stub+16,struct.pack('<d',seconds));x.mem_write(stack,w(stop,b,b+0x2000));trace=[]
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,196))+w(len(trace))+b''.join(w(*row) for row in trace)+bytes((16-len(trace))*12)
 assert got==want,('NXDK',case,got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--pain'],input=b''.join(inputs))
assert actual==b''.join(expected),'PC mismatch'
report=dict(result='PASS',cases=len(inputs),accepted=accepted,scope='Full original428740, real floating timer conversion573528, supplied query and downstream owner callbacks. Exact PC/NXDK retained state and ordered lazy-query/effect arguments, low-byte distinctions, custom/missing actions, and state mutation at each effect boundary. Does not implement gates, timers/RNG, motion or sound backends.')
(root/'artifacts/pain-reaction.json').write_text(json.dumps(report,indent=2));print(report)

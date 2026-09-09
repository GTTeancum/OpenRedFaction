"""Translation numeric integration vs unchanged original instructions."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
source=root/'Installed_Game/RF.exe';assert hashlib.sha256(source.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(0x30000000,65536);return u
u=machine(source);base=0x30000000;stack=base+50000;stop=base+64000;keys=base+4096;array=base+8192
rng=random.Random(0x4698ad);cases=[];expected=[];nonfinite_outcomes=0
for n in range(10100):
 points=[rng.uniform(-10000,10000) for _ in range(6)];timing=rng.choice([.125,2.,rng.uniform(.01,100)])
 accel=rng.choice([0.,-1.,.5,4.]);decel=rng.choice([0.,-1.,.5,4.,100.]);dt=rng.choice([0.,.25,1/60,rng.uniform(0,.1)])
 flags=rng.choice([0,0x400,0x2000,0x2400]);speed=rng.uniform(-1,100);elapsed=rng.choice([0.,.5,4.,rng.uniform(0,10)]);distance=rng.uniform(0,30000)
 if n>=10000:
  timing=[0.,-1.,1e-30,1e30][n%4];accel=decel=elapsed=0.;dt=.25
  if n%2==0:points[3:]=points[:3]
 wire=struct.pack('<10fI3f',*points,timing,accel,decel,dt,flags,speed,elapsed,distance);v=struct.unpack('<10fI3f',wire)
 before=bytearray([0xa5]*1024);struct.pack_into('<3I',before,0x29c,2,2,array);struct.pack_into('<2i',before,0x2f8,0,1);struct.pack_into('<I',before,0x318,flags)
 for offset,index in [(0x2f4,11),(0x300,12),(0x304,13)]:before[offset:offset+4]=wire[index*4:index*4+4]
 u.mem_write(base,bytes(before));u.mem_write(array,struct.pack('<2I',keys,keys+128))
 for i in range(2):
  key=bytearray(128);key[4:16]=wire[12*i:12*i+12];struct.pack_into('<4f',key,0x38,*[v[6],v[6],v[7],v[8]]);u.mem_write(keys+i*128,bytes(key))
 u.mem_write(0x5a4014,wire[36:40]);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4698ad,0x469b16,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x469b16
 after=bytes(u.mem_read(base,1024));out=b''.join(after[o:o+4] for o in [0x2f4,0x300,0x304])+bytes(u.mem_read(stack+0x10,4))
 for offset in [0x2f4,0x300,0x304]:before[offset:offset+4]=after[offset:offset+4]
 assert bytes(before)==after,(n,'object mutation');cases.append(wire)
 if all(math.isfinite(f) for f in struct.unpack('<4f',out)):expected.append(struct.pack('<i',0)+out)
 else:nonfinite_outcomes+=1;expected.append(struct.pack('<i',-2)+bytes([0xa5])*16)
for offset,value in [(0,math.nan),(24,math.inf),(36,math.nan)]:
 wire=bytearray(cases[0]);struct.pack_into('<f',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*16)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-integrate'],input=b''.join(cases));assert len(pc)==20*len(cases)
for n,want in enumerate(expected):assert pc[n*20:n*20+20]==want,(n,'PC',pc[n*20:n*20+20].hex(),want.hex(),cases[n].hex())
x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'_rf_group_translation_integrate\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 x.mem_write(base,wire);x.mem_write(base+128,bytes([0xa5])*16);x.mem_write(stack,struct.pack('<3I',stop,base,base+128));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+128,16));assert got==expected[n],(n,'NXDK',got.hex(),expected[n].hex())
report=dict(result='PASS',original_cases=10100,finite_matches=10100-nonfinite_outcomes,nonfinite_outcome_guards=nonfinite_outcomes,nonfinite_input_guards=3,scope='Original 4698ad..469b16 with real vector distance and speed clamp helpers; both directions and timing modes, acceleration/deceleration, varied positions/ticks/speed/elapsed/distance plus zero-length and zero/negative/extreme timing. Finite original results match PC/NXDK bitwise; whole object writes checked. Nonfinite results are port errors preserving output, not original behavior. Not timer, trigger, pose or arrival integration; full zero/nonfinite tick paths still need recovery.')
(root/'artifacts/group-integration-verification.json').write_text(json.dumps(report,indent=2));print(report)

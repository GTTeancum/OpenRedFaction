"""Full resolved-player 40db70: only player lookup and rand outputs are fixtures."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;context=base+0x2000;camera=base+0x4000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);u.mem_map(base,0x10000);return u
u=machine(exe);draws=[];used=0
def hook(m,address,size,data):
 global used
 if address not in (0x4a5b70,0x57312d):return
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if address==0x4a5b70:value=base
 else:value=draws[used];used+=1
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x4fae00);inputs=[];expected=[];active_count=0
for i in range(1800):
 now=100;deadline=[99,100,101,1099,1100,1101,-1][i%7];strength=rng.choice([0.,.01,1.,2.,-1.,rng.uniform(-3,3)])
 orientation=[rng.uniform(-1,1) for _ in range(9)]
 if i%5==0:orientation=[1,0,0,0,1,0,0,0,1]
 if i%11==0:orientation[3:6]=[0,0,0]
 if i%13==0:orientation[:6]=[0]*6
 if i%17==0:orientation[6:]=[0,1,0]
 draws=[rng.choice([0,32767,rng.randrange(32768)]) for _ in range(2)];used=0
 raw=f(strength,2)+struct.pack('<i',deadline);ori=f(*orientation);inputs.append(raw+w(now,*draws)+ori)
 u.mem_write(base+0x8b4,raw);u.mem_write(camera+0x7e0,ori);u.mem_write(context,w(camera,0));u.mem_write(0x5a3ed8,w(now));u.mem_write(stack,w(stop,context));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x40db70,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;assert used in (0,2);active_count+=bool(used)
 expected.append(w(0)+bytes(u.mem_read(base+0x8b4,12))+bytes(u.mem_read(camera+0x7e0,36))+w(int(bool(used))))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_eye_probe.exe'),'--effect-apply'],input=b''.join(inputs))
for i,e in enumerate(expected):assert actual[i*56:(i+1)*56]==e,('PC mismatch',i,actual[i*56:(i+1)*56].hex(),e.hex())
x=machine(ROOT/'build/xbox/main.exe');entry=int(re.search(r'_rf_camera_effect_apply\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
for i,payload in enumerate(inputs):
 x.mem_write(base,payload[:12]);x.mem_write(base+0x100,payload[24:]);x.mem_write(base+0x200,w(99));now,d0,d1=struct.unpack('<3I',payload[12:24])
 x.mem_write(stack,w(stop,base,now,d0,d1,base+0x100,base+0x200));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,12))+bytes(x.mem_read(base+0x100,36))+bytes(x.mem_read(base+0x200,4));assert got==expected[i],('NXDK mismatch',i,got.hex(),expected[i].hex())
report=dict(result='PASS',pc_cases=len(inputs),nxdk_cases=len(inputs),active_cases=active_count,scope='Complete original 40db70 with unchanged cone/basis/normalization/transform/rebuild code. Resolved entity and two rand values supplied at boundaries; RNG state ownership and camera rendering excluded.')
(ROOT/'artifacts/camera-effect-apply-verification.json').write_text(json.dumps(report,indent=2));print(report)

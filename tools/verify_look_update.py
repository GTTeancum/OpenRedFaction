"""Execute original look scalar path; compare PC and optionally compiled NXDK.
Stops before matrix building: does not claim a complete player-look update.
"""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x30000000;info=base+0x2000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);u.mem_map(base,0x10000);return u
exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);rng=random.Random(0x49de50);inputs=[];expected=[]
offsets=[0x708,0x70c,0x710,0x728,0x724,0x864,0x868,0x86c,0x87c,0x880,0x884,0x150,0x154,0x158]
for i in range(1200):
 values=[rng.uniform(-3,3) for _ in offsets];speed=rng.uniform(.1,10);dt=rng.choice([1/30,1/60,.1,1])
 values[6]=rng.choice([rng.uniform(-6300,6300),0,6.2831854820251465,-6.2831854820251465])
 if i%5==0:values[0]=values[1]=values[3]=values[4]=0
 payload=f(*values,speed,dt);inputs.append(payload)
 for j,offset in enumerate(offsets):u.mem_write(base+offset,payload[j*4:j*4+4])
 u.mem_write(base+0x294,w(info));u.mem_write(info+0x64,payload[56:60]);u.mem_write(base+0x1b0,payload[60:64]);u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x49de50,0x49dfcd,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x49dfcd
 expected.append(w(0)+b''.join(bytes(u.mem_read(base+offset,4)) for offset in offsets))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_eye_probe.exe'),'--look'],input=b''.join(inputs))
assert len(actual)==len(expected)*60
for i,e in enumerate(expected):assert actual[i*60:(i+1)*60]==e,('PC mismatch',i,struct.unpack('<15I',actual[i*60:(i+1)*60]),struct.unpack('<15I',e))
report=dict(result='PASS',original_sha256=digest,pc_cases=len(inputs),scope='Unmodified original 49de50..49dfcd scalar path including vector clearing. Matrix building and physics commit excluded; no input binding or live XEMU claim.')
if '--nxdk' in sys.argv:
 x=machine(ROOT/'build/xbox/main.exe');mapping=(ROOT/'build/xbox/main.map').read_text();entry=int(re.search('_rf_look_update'+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
 for i,payload in enumerate(inputs):
  x.mem_write(base,payload[:56]);x.mem_write(stack,w(stop,base)+payload[56:]);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
  got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,56));assert got==expected[i],('NXDK mismatch',i)
 report['nxdk_cases']=len(inputs)
(ROOT/'artifacts/look-update-verification.json').write_text(json.dumps(report,indent=2));print(report)

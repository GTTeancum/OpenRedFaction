"""Original skeletal bone query vs shared output using sampled miner burn bones."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda values:struct.pack('<'+'f'*len(values),*values)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0x30000000,65536);return u
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');b=0x30000000;desc=b+0x4000;handle=b+0x5000;output=b+0xa000;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'_rf_model_query_bone\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x503230);cases=[];expected=[];real=[]
for i in range(256):cases.append(w(25,(-1,0,8,11,12,15,24)[i%7])+f([rng.uniform(-5,5) for _ in range(25*12)]))
miner=next(r for r in json.loads((root/'artifacts/burn-model-assets/report.json').read_text())['records'] if r['model'].lower()=='miner.v3c');indices=miner['indices'];assert len(indices)==4 and min(indices)>=0
for motion in ('ult2_stand.rfa','ult2_crouch.rfa'):
 ticks=list(range(0,4001,125));poses=subprocess.check_output([str(root/'build/pc/Release/rf_skeleton_probe.exe'),str(root/'Installed_Game/meshes.vpp'),str(root/'Installed_Game/motions.vpp'),'miner.v3c',motion],input=w(*ticks));stride=len(poses)//len(ticks);count=stride//48-1;assert stride==(count+1)*48
 for frame,tick in enumerate(ticks):
  for index in indices:
   real.append(dict(case=len(cases),motion=motion,tick=tick,bone=index));cases.append(w(count,index)+poses[frame*stride:frame*stride+count*48])
for wire in cases:
 count,index=struct.unpack('<2I',wire[:8]);u.mem_write(b,bytes(0x3000));u.mem_write(b,wire[8:]);u.mem_write(b+0x1d50,w(desc));u.mem_write(desc+0x48,w(count));u.mem_write(handle,w(2,b));u.mem_write(output,bytes([0xa5])*48)
 u.mem_write(stack,w(stop,handle,output,output+12,index));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x503230,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected.append(w(0)+bytes(u.mem_read(output,48)))
for row in real:row['position']=struct.unpack('<3f',expected[row['case']][4:16])
original_count=len(cases)
for index in (-2,25):wire=bytearray(cases[1]);wire[4:8]=w(index);cases.append(bytes(wire));expected.append(w(-4)+bytes([0xa5])*48)
for offset in range(8,56,4):
 for value in (0x7fc00000,0x7f800000):wire=bytearray(cases[1]);wire[offset:offset+4]=w(value);cases.append(bytes(wire));expected.append(w(-2)+bytes([0xa5])*48)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--bone-query'],input=b''.join(cases));assert actual==b''.join(expected),'PC mismatch'
from unicorn.x86_const import UC_X86_REG_EAX
for i,(wire,want) in enumerate(zip(cases,expected)):
 count,index=struct.unpack('<2I',wire[:8]);x.mem_write(b,wire[8:]);x.mem_write(output,bytes([0xa5])*48);x.mem_write(stack,w(stop,b,count,index,output));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(output,48))==want,('NXDK',i)
report=dict(result='PASS',original_cases=original_count,guard_cases=len(cases)-original_count,real_queries=len(real),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original503230/5012a0 kind2/51c590/51b2e0 with cached pose matrices, no hooks. Shared PC/NXDK exact position/basis outputs;264 real miner burn-bone queries from shared standing/crouching sampled poses. Bounds/nonfinite guards preserve output. No lazy original animation advancement, virtual bones or live emitter integration.',real=real)
(root/'artifacts/burn-pose-query.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='real'})

"""Original4e5c60 sampling with supplied UVs/bitmap lock vs shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data);m.mem_map(0x30000000,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary)
b=0x30000000;world=b;face=b+0x1000;array=b+0x2000;descriptor=b+0x3000;texture=b+0x4000;pixels=b+0x5000;out=b+0x6000;view=b+0x7000;uvptr=b+0x8000;stack=b+0xe000;stop=b+0xf000
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
uv=(0,0);available=1;pitch=0;trace=[]
def hook(m,address,size,unused):
 sp=m.reg_read(UC_X86_REG_ESP)
 def ret(value=0,pop=0):
  m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,get(m,sp))
 if address==0x4e49d0:
  assert m.reg_read(UC_X86_REG_ECX)==descriptor
  m.mem_write(get(m,sp+8),struct.pack('<f',uv[0]));m.mem_write(get(m,sp+12),struct.pack('<f',uv[1]));trace.append('uv');ret(0,12)
 elif address==0x50e2e0:
  assert get(m,sp+4)==123 and get(m,sp+8)==0 and get(m,sp+16)==0
  target=get(m,sp+12);m.mem_write(target+12,w(pixels));m.mem_write(target+24,w(pitch));trace.append('lock');ret(available)
 elif address==0x50e310:trace.append('unlock');ret()
u.hook_add(UC_HOOK_CODE,hook)
entry=int(re.search(r'\s_rf_lightmap_sample_1555\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inputs=[];outputs=[];rng=random.Random(0x4e5c60);row_spills=0;unavailable=0

def shared(wire,want):
 width,height,stride,length,present=struct.unpack_from('<5I',wire)
 x.mem_write(pixels,wire[28:]);x.mem_write(view,w(pixels if present else 0,width,height,stride,length));x.mem_write(uvptr,wire[20:28]);x.mem_write(out,w(0x12345678))
 x.mem_write(stack,w(stop,view,uvptr,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,4));assert actual==want,('NXDK',len(inputs),actual.hex(),want.hex())
 inputs.append(wire);outputs.append(want)

for case in range(2048):
 width=rng.choice([1,3,7,16]);height=rng.choice([2,4,7]);pitch=width*2+rng.choice([0,2,4]);length=pitch*height
 uv=tuple(struct.unpack('<2f',struct.pack('<2f',rng.choice([0,0.125,0.3,0.99999,1]),rng.choice([0,0.2,0.5,0.8]))))
 available=int(case%11!=0);missing=case%17==0
 raw=bytes(rng.randrange(256) for _ in range(512));trace.clear()
 u.mem_write(world+0xc8,w(array));u.mem_write(array,w(descriptor));u.mem_write(descriptor+12,w(texture));u.mem_write(texture+4,w(width,height,0,123))
 u.mem_write(face+0x36,struct.pack('<h',-1 if missing else 0));u.mem_write(pixels,raw);u.mem_write(out,w(0x12345678))
 u.mem_write(stack,w(stop,out,face,uvptr));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,world);u.reg_write(UC_X86_REG_FPCW,0x37f)
 offset=int(width*uv[0])*2+int(height*uv[1])*pitch
 if offset+2>length:continue # Out-of-buffer original reads are outside this comparison.
 u.emu_start(0x4e5c60,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 present=available and not missing;unavailable+=not present
 assert trace==([] if missing else ['uv','lock']+(['unlock'] if available else []))
 color=bytes(u.mem_read(out,4))
 if not present:assert color==w(0xffffffff)
 else:
  pixel=struct.unpack_from('<H',raw,offset)[0]
  assert color==bytes([((pixel>>10)&31)*8,((pixel>>5)&31)*8,(pixel&31)*8,255])
  row_spills+=uv[0]==1
 shared(w(width,height,pitch,length,int(present))+struct.pack('<2f',*uv)+raw,w(0)+color)
original_cases=len(inputs)
# Guard last-row overread, nonfinite/out-of-range UV, and malformed pitch.
for uv_guard,stride in [((0,1),8),((float('nan'),0),8),((-0.1,0),8),((0,0),7)]:
 shared(w(4,2,stride,16,1)+struct.pack('<2f',*uv_guard)+bytes(512),w(-4,0x12345678))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--lightmap-sample'],input=b''.join(inputs));assert actual==b''.join(outputs),'PC mismatch'
report=dict(result='PASS',original_cases=original_cases,port_guards=4,unavailable=unavailable,row_edge_samples=row_spills,original_sha256=digest,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Original4e5c60 texture selection, coordinate truncation, pitched1555 fetch, RGBA conversion and lock/unlock ordering; UV calculation and bitmap lock supplied. Shared PC/NXDK sampler matches. Missing face lightmap/lock returns white. In-buffer u==1 addressing retained; out-of-buffer reads rejected only by port guards. No world-to-UV, authored texture format ownership or live effect binding.')
(root/'artifacts/lightmap-sample.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

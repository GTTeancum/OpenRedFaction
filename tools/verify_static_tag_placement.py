"""Compare static tag placement with unhooked original 0x5034f0."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,handle,data,stack,stop=[0x30000000+i*0x100000 for i in range(6)]
for a in (obj,desc,handle,data,stack,stop): u.mem_map(a,65536)
u.mem_write(handle,struct.pack('<2I',1,obj));u.mem_write(obj+0x48,struct.pack('<2I',1,desc));u.mem_write(desc+0x8c,struct.pack('<I',desc+0x200));u.mem_write(desc+0x204,struct.pack('<I',desc+0x300));u.mem_write(desc+0x310,struct.pack('<2I',desc+0x1000,1))
rng=random.Random(0x53c1c4);cases=[]
for i in range(512):
 q=[rng.uniform(-2,2) for _ in range(4)] if i else [0,0,0,0]
 p=[rng.uniform(-20,20) for _ in range(3)]
 basis=[rng.uniform(-2,2) for _ in range(9)];pos=[rng.uniform(-100,100) for _ in range(3)]
 cases.append(struct.pack('<19f',*(q+p+basis+pos)))
run=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--static-tag-place'],input=b''.join(cases))
failures=[];expected=[]
for i,raw in enumerate(cases):
 u.mem_write(desc+0x1000,bytes(68)+raw[:28]+struct.pack('<i',123));u.mem_write(data,raw[28:])
 u.mem_write(stack+64000,struct.pack('<7I',stop,handle,0,data,data+36,data+128,data+164))
 u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x5034f0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 want=bytes(u.mem_read(data+128,48));expected.append(want);status,=struct.unpack_from('<i',run,i*52);got=run[i*52+4:(i+1)*52]
 if status or want!=got:failures.append(dict(index=i,status=status,expected=want.hex(),actual=got.hex()))
print(dict(cases=len(cases),failures=len(failures),examples=failures[:1]))
assert not failures

# Actual NXDK composition, without replacing math or query helpers.
import re
from unicorn.x86_const import UC_X86_REG_EAX
pe=pefile.PE(str(root/'build/xbox/main.exe'));im=pe.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(pe.OPTIONAL_HEADER.ImageBase,im);x.mem_map(obj,65536)
entry=int(re.search(r'\s_rf_static_model_tag_place\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
owner=obj;tag=obj+0x100;inp=obj+0x200;out=obj+0x300;sp=obj+0xf000;end=obj+0xff00
x.mem_write(owner,w(tag,1,116))
def call(index):
 x.mem_write(sp,w(end,owner,index,inp,inp+36,out));x.reg_write(UC_X86_REG_ESP,sp);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,end,count=10000);assert x.reg_read(UC_X86_REG_EIP)==end;return x.reg_read(UC_X86_REG_EAX)
for raw,want in zip(cases,expected):
 x.mem_write(tag,bytes(72)+raw[:28]+w(123));x.mem_write(inp,raw[28:]);assert call(0)==0
 assert bytes(x.mem_read(out,48))==want
for index in (-1,1,0x7fffffff):
 x.mem_write(out,b'\xa5'*48);assert call(index)!=0 and bytes(x.mem_read(out,48))==b'\xa5'*48
for offset in (72,88):
 x.mem_write(tag+offset,w(0x7fc00000));x.mem_write(out,b'\xa5'*48)
 assert call(0)!=0 and bytes(x.mem_read(out,48))==b'\xa5'*48
 x.mem_write(tag,bytes(72)+cases[-1][:28]+w(123))
report=dict(result='PASS',cases=len(cases),nxdk_guards=5,scope='Unhooked static5034f0/503230/5012a0/53c1c4 and quaternion/world math versus shared PC and compiled NXDK, exact48-byte poses at x87CW0x27f. Supplied first-submesh attachment arrays, nonnormalized/zero quaternions and ignored parent123. NXDK invalid indices/nonfinite pose preserve output. No live glare, native Xbox or model loading claim.')
(root/'artifacts/static-tag-placement.json').write_text(json.dumps(report,indent=2));print(report)

"""Prepared original animation decision versus PC and NXDK machine code.

Distance and predicate are supplied boundaries; actual descriptor and model-kind
accessors execute. Animation dispatch is observed, not executed.
"""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *x:struct.pack('<'+'I'*len(x),*x)
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 b=p.OPTIONAL_HEADER.ImageBase;u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,0x10000)
 return u
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);nx=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_animation_should_advance\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
# Process-local replacement distance boundary: fld binary32; ret.
u.mem_write(0x5182f0,b'\xd9\x05'+pack(base+0x7000)+b'\xc3')
u.mem_write(0x6fc4d8,b'\0')
advanced=False;predicate=0

def hook(uc,address,size,context):
 global advanced
 if address not in (0x427020,0x503360):return
 sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0]
 if address==0x427020:uc.reg_write(UC_X86_REG_EAX,predicate)
 else:advanced=True
 uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
cases=[]
for present,kind,desc,flag,flags,state,detail,pred,distance in itertools.product(
 (0,1),(2,3),(0,1),(0,1,256),(0,0x80000000),(0,1,2,13),(-1,2,3),(0,1,2,257),(0.,44.999996185302734,45.,45.000003814697266,100.)):
 cases.append(struct.pack('<5I2iIf',present,kind,desc,flag,flags,state,detail,pred,distance))
pc=subprocess.run([str(root/'build/pc/Release/rf_entity_probe.exe'),'--animation-gate'],input=b''.join(cases),capture_output=True,check=True).stdout
assert len(pc)==len(cases)*4
count=0
for k,raw in enumerate(cases):
 present,kind,desc,flag,flags,state,detail,predicate,distance=struct.unpack('<5I2iIf',raw)
 u.mem_write(base,bytes(0x1500));u.mem_write(base,pack(base+0x2000 if desc else 0))
 u.mem_write(base+0x80,pack(base+0x4000 if present else 0));u.mem_write(base+0x4000,pack(kind,base+0x5000))
 u.mem_write(base+0x2160,bytes([flag&255]));u.mem_write(base+0x814,pack(flags));u.mem_write(base+0x520,struct.pack('<i',state))
 u.mem_write(base+0x294,pack(base+0x3000));u.mem_write(base+0x43b8,struct.pack('<i',detail));u.mem_write(base+0x7000,raw[-4:])
 advanced=False;u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x41dbea,0x41dd49,count=500)
 assert u.reg_read(UC_X86_REG_EIP)==0x41dd49 and u.reg_read(UC_X86_REG_ESP)==stack-4
 expected=struct.pack('<I',advanced);assert pc[k*4:k*4+4]==expected,('PC',k,struct.unpack('<5I2iIf',raw),advanced)
 nx.mem_write(base,raw);nx.mem_write(stack,pack(stop,base));nx.reg_write(UC_X86_REG_ESP,stack)
 nx.emu_start(entry,stop,count=500)
 assert nx.reg_read(UC_X86_REG_EIP)==stop and nx.reg_read(UC_X86_REG_EAX)==int(advanced),('NXDK',k)
 count+=advanced
report=dict(result='PASS',cases=len(cases),advanced=count,original_sha256=digest,scope='Original41dbea..41dd49 with actual model-kind and descriptor getters; supplied5182f0 binary32 distance and427020 predicate;503360 advance dispatch intercepted. PC and NXDK decisions match. Excludes alternate-view distance calculation, earlier actor entry gates and live playback integration.')
(root/'artifacts/npc-animation-gate.json').write_text(json.dumps(report,indent=2));print(report)

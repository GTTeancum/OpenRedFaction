"""Compare entity-local primary view with its unchanged original update block."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;position_address=base+256;orientation_address=base+512
def invalid(u,access,address,size,value,data):
    print('invalid memory',hex(u.reg_read(UC_X86_REG_EIP)),hex(address),size);return False
u.hook_add(UC_HOOK_MEM_INVALID,invalid)
rng=random.Random(0x547485);cases=[];expected=[]
for n in range(2000):
    values=[rng.randint(-4096,4096)/32 for _ in range(23)];flags=[rng.randrange(2) for _ in range(5)]
    position=[rng.randint(-4096,4096)/32 for _ in range(3)];orientation=[rng.randint(-64,64)/16 for _ in range(9)]
    if n%4==0:orientation=[1,0,0,0,1,0,0,0,1]
    if n%4==1:orientation=[0,0,-1,0,1,0,1,0,0]
    view=struct.pack('<23f5I',*values,*flags)
    cases.append(view+struct.pack('<12f',*position,*orientation))
    u.mem_write(0x1818690,view[:12]);u.mem_write(0x18186c8,view[12:48])
    u.mem_write(position_address,struct.pack('<3f',*position));u.mem_write(orientation_address,struct.pack('<9f',*orientation))
    u.mem_write(esp,bytes(512));u.mem_write(esp+0x9c,struct.pack('<2I',position_address,orientation_address))
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.reg_write(UC_X86_REG_ECX,0x1818690) # Set by 0x547479 before the block.
    u.emu_start(0x547485,0x5474cc,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x5474cc
    expected.append(bytes(u.mem_read(0x1818690,12))+bytes(u.mem_read(0x18186c8,36))+view[48:])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--local-view'],input=b''.join(cases))
assert len(actual)==len(cases)*112
for n,reference in enumerate(expected):
    observed=actual[n*112:(n+1)*112]
    assert observed==reference,(n,next((i,a,b) for i,(a,b) in enumerate(zip(observed,reference)) if a!=b))
report=dict(result='PASS',cases=len(cases),scope='Unchanged 0x547485..0x5474cc and complete subtract/rotate/transpose/matrix-product/copy callees; dyadic positions and matrices, identity and quarter-turn orientations, all 112 view bytes exact; no global stack, secondary view state or placed scene claim')
(root/'artifacts/model-local-view-verification.json').write_text(json.dumps(report,indent=2));print(report)

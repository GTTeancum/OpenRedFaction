"""Compare triangle facing with its unchanged original renderer branch."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;triangle=base+256
def stop(u,address,size,user):
    if address in (0x52f57b,0x52f890):u.emu_stop()
u.hook_add(UC_HOOK_CODE,stop)
rng=random.Random(524);cases=[];expected=[]
for n in range(2000):
    values=[rng.randint(-1000,1000)/16 for _ in range(15)];flags=32 if n%5==0 else 0;perspective=n%2
    if n%7==0:values[6:9]=values[:3]
    if n%19==0:values[n%15]=float('nan')
    if n%23==0:values[n%15]=float('inf')
    case=struct.pack('<15fII',*values,flags,perspective);cases.append(case)
    u.mem_write(0x1bf7000,case[:36]);u.mem_write(0x1818690,case[36:48]);u.mem_write(0x18186e0,case[48:60]);u.mem_write(0x5a4d19,bytes([perspective]))
    u.mem_write(triangle,struct.pack('<4H',0,1,2,flags));u.mem_write(esp,bytes(1024));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EDI,triangle)
    u.emu_start(0x52f4e8,0,count=10000);end=u.reg_read(UC_X86_REG_EIP);assert end in (0x52f57b,0x52f890)
    expected.append(struct.pack('<I',end==0x52f57b))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--triangle-facing'],input=b''.join(cases))
assert actual==b''.join(expected)
report=dict(result='PASS',cases=len(cases),accepted=sum(struct.unpack('<I',x)[0] for x in expected),
    scope='Unchanged 0x52f4e8 facing branch with full subtract/cross/dot/facing callees; both projection modes, flag bypass, degenerates, NaN and infinity; supplied world vertices/view globals, no clipping or submission claim')
(root/'artifacts/model-triangle-facing-verification.json').write_text(json.dumps(report,indent=2));print(report)

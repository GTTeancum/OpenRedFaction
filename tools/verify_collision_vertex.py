"""Execute the unmodified collision skinning loop and all its vector callees."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;obj=base+4096;links=base+2048
rng=random.Random(540344);cases=[];expected=[]
for n in range(2000):
    position=struct.pack('<3f',*(rng.randint(-200,200)/16 for _ in range(3)))
    weights=bytes([rng.randrange(256) for _ in range(4)])
    if n%5==0:weights=weights[:n%4]+b'\0'+weights[n%4+1:]
    bones=bytes(rng.randrange(4) for _ in range(4))
    matrices=struct.pack('<48f',*(rng.randint(-100,100)/16 for _ in range(48)))
    cases.append(position+weights+bones+matrices)
    u.mem_write(esp,bytes(256));u.mem_write(esp+0x40,position)
    u.mem_write(esp+0xc0,struct.pack('<I',obj));u.mem_write(obj+0x960,matrices);u.mem_write(links,weights+bones)
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EDI,links)
    u.emu_start(0x54e344,0x54e3c0,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x54e3c0
    expected.append(struct.pack('<i',0)+bytes(u.mem_read(esp+0x4c,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--collision-vertex'],input=b''.join(cases))
assert actual==b''.join(expected)
# Invalid active bone rejects atomically; a zero weight stops before its bone.
bad=bytearray(cases[0]);bad[12:16]=bytes([255,1,0,0]);bad[16:20]=bytes([0,255,0,0])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--collision-vertex'],input=bad)
assert actual==struct.pack('<i3f',-4,99,99,99)
bad[12]=0
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--collision-vertex'],input=bad)
assert actual==bytes(16)
report=dict(result='PASS',cases=len(cases),port_bounds_cases=2,scope='Unchanged 0x54e344..0x54e3c0 collision deformation block and complete original callees, exact dyadic-fixture outputs; supplied prepared matrices, no rendering or pose preparation claim')
(root/'artifacts/collision-vertex-verification.json').write_text(json.dumps(report,indent=2));print(report)

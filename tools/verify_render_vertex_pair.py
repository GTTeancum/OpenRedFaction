"""Compare fresh-vertex rendering deformation against its unchanged x86 block."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;obj=base+4096;batch=base+1024;links=base+2048;pos=base+256;second=pos+12
rng=random.Random(520099);cases=[];expected=[]
for n in range(2000):
    vectors=struct.pack('<6f',*(rng.randint(-200,200)/16 for _ in range(6)))
    weights=bytes(rng.randrange(256) for _ in range(4))
    if n%5==0:weights=weights[:n%4]+b'\0'+weights[n%4+1:]
    bones=bytes(rng.randrange(4) for _ in range(4))
    matrices=struct.pack('<48f',*(rng.randint(-100,100)/16 for _ in range(48)))
    cases.append(vectors+weights+bones+matrices)
    u.mem_write(esp,bytes(1024));u.mem_write(pos,vectors);u.mem_write(links,weights+bones)
    u.mem_write(obj+0x960,matrices);u.mem_write(batch+0x1c,struct.pack('<I',links))
    for offset,value in ((0x34,0x1c25e04),(0x38,batch),(0x390,obj),(0x2c,0x1bf7008)):
        u.mem_write(esp+offset,struct.pack('<I',value))
    for register,value in ((UC_X86_REG_ESP,esp),(UC_X86_REG_ESI,0),(UC_X86_REG_EDI,pos),(UC_X86_REG_EBP,second)):u.reg_write(register,value)
    u.emu_start(0x52ee9d,0x52f154,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52f154
    expected.append(bytes(4)+bytes(u.mem_read(0x1bdb2b0,12))+bytes(u.mem_read(0x1c25e00,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--render-vertex-pair'],input=b''.join(cases))
assert actual==b''.join(expected)
bad=bytearray(cases[0]);bad[24:28]=bytes([255,1,0,0]);bad[28:32]=bytes([0,255,0,0])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--render-vertex-pair'],input=bad)
assert actual==struct.pack('<i6f',-4,*([99]*6))
bad[24]=0
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--render-vertex-pair'],input=bad)
assert actual==bytes(28)
report=dict(result='PASS',cases=len(cases),port_bounds_cases=2,scope='Unchanged 0x52ee9d..0x52f154 fresh-vertex deformation block, exact dyadic-fixture outputs; prepared streams/matrices supplied; no duplicate reuse, camera, projection, lighting or draw claim')
(root/'artifacts/render-vertex-pair-verification.json').write_text(json.dumps(report,indent=2));print(report)

"""Compare complete original model-lighting helper including NaN inputs."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;vector=base+100;output=base+200;stop=base+60000
rng=random.Random(5200);cases=[];expected=[]
for n in range(2000):
    values=[rng.randint(-32,32)/16 for _ in range(3)]
    if n<20:values[n%3]=float('nan') if n<10 else float('inf')
    lights=[]
    for i in range(3):lights.extend([rng.randint(-32,32)/16 for _ in range(3)]+[rng.randint(-64,256)/2 for _ in range(3)])
    ambient=[rng.randint(-128,600)/2 for _ in range(3)]
    raw=struct.pack('<24f',*(values+lights+ambient));cases.append(raw)
    u.mem_write(vector,raw[:12]);u.mem_write(0x1c3d500,raw[12:]);u.mem_write(output,b'\xa5'*3)
    u.mem_write(esp,struct.pack('<4I',stop,0,output,vector));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x52fcf0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected.append(bytes(4)+bytes(u.mem_read(output,3)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--vertex-lighting'],input=b''.join(cases))
assert actual==b''.join(expected),next(i for i in range(len(cases)) if actual[i*7:i*7+7]!=expected[i])
report=dict(result='PASS',cases=len(cases),nonfinite_vector_cases=20,scope='Complete original 0x52fcf0 with unchanged dot-product and byte-conversion callees; dyadic fixtures and nonfinite vectors; light/normal setup and draw integration excluded')
(root/'artifacts/vertex-lighting-verification.json').write_text(json.dumps(report,indent=2));print(report)

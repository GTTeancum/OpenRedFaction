"""Execute original local-light selection including real vector callees."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;pos=base+100;objects=[base+4096+i*256 for i in range(8)]
u.mem_write(0xc9687c,struct.pack('<I',8));u.mem_write(0xc4d588,struct.pack('<8I',*objects))
rng=random.Random(5200);cases=[];expected=[];selected=0
for n in range(2000):
    p=[rng.randint(-32,32)/4 for _ in range(3)];raw=struct.pack('<3f',*p);u.mem_write(pos,raw)
    for i,obj in enumerate(objects):
        xyz=[rng.randint(-32,32)/4 for _ in range(3)];radius=rng.randint(0,256);enabled=rng.randrange(2)
        if n%7==0:xyz=p.copy();radius=0;enabled=1 # exact-radius ties
        if n%11==0:enabled=0
        if n%13==0 and i==0:radius=float('nan')
        record=struct.pack('<4fI',*xyz,radius,enabled);raw+=record
        u.mem_write(obj,bytes(256));u.mem_write(obj+12,record[:12]);u.mem_write(obj+0x80,record[12:16]);u.mem_write(obj+0x4d,bytes([enabled]))
    cases.append(raw);u.mem_write(esp,bytes(128));u.mem_write(esp+0x4c,struct.pack('<I',pos));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x52dcaf,0x52dd48,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52dd48
    address=u.reg_read(UC_X86_REG_EBP);index=objects.index(address) if address else -1;selected+=index>=0
    expected.append(struct.pack('<i',index)+bytes(u.mem_read(esp+0x2c,12))+bytes(u.mem_read(esp+0x10,4)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--choose-light'],input=b''.join(cases))
assert actual==b''.join(expected),next(i for i in range(len(cases)) if actual[i*20:i*20+20]!=expected[i])
report=dict(result='PASS',cases=len(cases),selected=selected,scope='Unchanged 0x52dcaf..0x52dd48 plus complete vector callees; ties, boundary radii, disabled lights and NaN radii included; global gate/direction/attenuation excluded')
(root/'artifacts/model-light-choice-verification.json').write_text(json.dumps(report,indent=2));print(report)

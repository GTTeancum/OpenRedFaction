"""Compare complete effect-pair switching with original instructions."""
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
objects,stack=0x30000000,0x30100000
for a in (objects,stack):u.mem_map(a,65536)
rng=random.Random(0x48f130);cases=[]
for k in range(4000):
    header=struct.pack('<iIii',0,rng.choice([0,1,2,255,256]),rng.choice([0,1,2,-1]),rng.choice([0,1072800000,rng.randrange(1072800001)]))
    values=b''.join(struct.pack('<4Bi',rng.choice([0,1,2,255]),17,33,49,rng.choice([-1,0,99,1072800000])) for _ in range(4))
    slots=struct.pack('<4i',*[rng.choice([-1,0,1,2,3]) for _ in range(4)])
    cases.append(header+values+slots)
out=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe')],input=b''.join(cases));assert len(out)==36*len(cases)
for k,wire in enumerate(cases):
    index,override,enabled,now=struct.unpack_from('<iIii',wire)
    u.mem_write(0x64ecb9,bytes([override&255]));u.mem_write(0x5a3ed8,struct.pack('<i',now))
    for i in range(4):
        u.mem_write(objects+i*1024+0x140,wire[16+i*8:20+i*8]);u.mem_write(objects+i*1024+0x154,wire[20+i*8:24+i*8])
    slots=struct.unpack_from('<4i',wire,48)
    u.mem_write(0x75ec48,struct.pack('<4I',*[objects+i*1024 if i>=0 else 0 for i in slots]))
    stop=stack+65000;u.mem_write(stack+64000,struct.pack('<Iii',stop,index,enabled));u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x48f130,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=bytes(4)+b''.join(bytes(u.mem_read(objects+i*1024+0x140,4))+bytes(u.mem_read(objects+i*1024+0x154,4)) for i in range(4))
    assert out[k*36:(k+1)*36]==expected,k
for index in (-1,1):
    wire=struct.pack('<i',index)+cases[0][4:];out=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe')],input=wire)
    assert struct.unpack_from('<i',out)[0]==-4 and out[4:]==wire[16:48]
report=dict(result='PASS',cases=len(cases),bounds_rejections=2,scope='Complete original 48f130 with unmodified 4973b0/4973d0 and timer callees, null pairs and aliased pointers, exact-byte enabling and int-boolean toggles; rendering/ownership excluded')
(root/'artifacts/effect-switch-verification.json').write_text(json.dumps(report,indent=2));print(report)

"""Verify the original first-person pose copy before camera effects/commit."""
import hashlib, json, random, re, struct, subprocess, sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=ROOT/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;source=base+0x2000;stack=base+0xe000;stop=base+0xf000
word=lambda *v:struct.pack('<%dI'%len(v),*v)
def machine(path):
    pe=pefile.PE(str(path));image=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(image)+4095)//4096*4096);u.mem_write(origin,image);u.mem_map(base,0x10000)
    return u
u=machine(exe);x=machine(ROOT/'build/xbox/main.exe')
entry=int(re.search(r'_rf_first_person_pose_copy\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x40d88c)
inputs=[struct.pack('<21f',*[rng.uniform(-10000,10000) for _ in range(21)]) for _ in range(256)]
inputs.extend([word(*([0]*21)),word(*([0x80000000]*21))])
expected=[]
for raw in inputs:
    u.mem_write(base,bytes(0x4000));u.mem_write(source+0x7d4,raw[:12]);u.mem_write(source+0x48,raw[12:48]);u.mem_write(source+0x7e0,raw[48:])
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EDI,source)
    u.emu_start(0x40d88c,0x40d8be,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x40d8be and u.reg_read(UC_X86_REG_ESP)==stack
    wanted=bytes(u.mem_read(base+0x3c,48))+bytes(u.mem_read(base+0x7e0,36));assert wanted==raw
    expected.append(word(0)+wanted)
    for alias in (False,True):
        output=source if alias else base
        x.mem_write(source,raw)
        if not alias:x.mem_write(output,bytes([0xa5])*84)
        x.mem_write(stack,word(stop,source,source+12,source+48,output));x.reg_write(UC_X86_REG_ESP,stack)
        x.emu_start(entry,stop,count=10000)
        assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
        assert bytes(x.mem_read(output,84))==wanted
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_eye_probe.exe'),'--camera-pose'],input=b''.join(inputs))
assert actual==b''.join(expected),'PC differs from original first-person copy'
bad=bytearray(inputs[0]);bad[0:4]=word(0x7fc00000)
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_eye_probe.exe'),'--camera-pose'],input=bad)
assert actual==struct.pack('<i',-2)+bytes([0xa5])*84
report=dict(status='PASS',original_cases=len(inputs),nxdk_cases=2*len(inputs),pc_cases=len(inputs),scope='40d88c..40d8be with original vector/matrix copy callees. Prepared player eye/body/eye-orientation inputs. Stops before 40db70 effects and 48a190 entity commit; no player handle resolution, eye derivation or first-person rendering.')
(ROOT/'artifacts/first-person-pose-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

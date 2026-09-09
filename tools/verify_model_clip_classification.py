"""Compare generated-vertex clip classification with the original wrapper/callees."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;record=base+256;stop=base+2048
rng=random.Random(518320);cases=[];expected=[];masks={}
for n in range(3000):
    data=bytearray(rng.randbytes(48));position=[rng.randint(-100,100)/16 for _ in range(3)]
    if n%11==0:position[0]=position[2]
    if n%13==0:position[1]=-position[2]
    if n%17==0:position[n%3]=float('nan')
    if n%19==0:position[n%3]=float('inf')
    if n%23==0:position[2]=0
    struct.pack_into('<3f',data,0,*position)
    far=rng.randint(-100,100)/16;flags=[n%2,0,(n//2)%2,(n//4)%2,0];mode=0x66 if n%5 else 0
    view=struct.pack('<23f5I',*([0]*22),far,*flags);cases.append(bytes(data)+view+struct.pack('<I',mode))
    u.mem_write(record,bytes(data));u.mem_write(0x17c7bcc,struct.pack('<I',mode));u.mem_write(0x1818b6c,struct.pack('<f',far))
    for address,value in [(0x5a4d19,flags[0]),(0x5a4d18,flags[2]),(0x1818b65,flags[3])]:u.mem_write(address,bytes([value]))
    u.mem_write(esp,struct.pack('<II',stop,record));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x518320,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(u.mem_read(record,48));expected.append(result)
    if mode==0x66:masks[result[24]]=masks.get(result[24],0)+1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--classify-clip'],input=b''.join(cases))
assert actual==b''.join(expected)
report=dict(result='PASS',cases=len(cases),active_mode_masks=masks,
    scope='Complete 0x518320 with unchanged 0x518bd0/0x5475d0; both mode gates, clip/perspective/far gates, boundary and non-finite inputs; all 48 bytes exact; camera-space input supplied, no polygon traversal')
(root/'artifacts/model-clip-classification-verification.json').write_text(json.dumps(report,indent=2));print(report)

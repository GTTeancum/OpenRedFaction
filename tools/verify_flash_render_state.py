"""Record full original HUD mode application at a fake D3D8 device."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe=pefile.PE(str(exe));blob=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(blob)+4095)//4096*4096);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,blob)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;device=base;vtable=base+0x100
u.mem_map(base,65536)
def word(a,v):u.mem_write(a,struct.pack('<I',v))
word(device,vtable);word(0x1cfcbe4,device)
for offset,entry in ((0xc8,stop+16),(0xfc,stop+32),(0xf4,stop+48)):word(vtable+offset,entry)
calls=[]
def hook(m,a,size,data):
    if a not in (stop+16,stop+32,stop+48):return
    n=4 if a==stop+32 else 3;sp=m.reg_read(UC_X86_REG_ESP)
    args=struct.unpack('<'+'I'*(n+1),m.mem_read(sp,(n+1)*4));assert args[1]==device
    calls.append((a-stop,args[2:]));m.reg_write(UC_X86_REG_EAX,0)
    m.reg_write(UC_X86_REG_ESP,sp+(n+1)*4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook)
# Prepared empty batch and initialized texture-stage owner indices. Actual
# negative-handle55cad0/55d250 executes; both stages are unbound at the device.
u.mem_write(0x1e652ed,b'\0\1');u.mem_write(0x1cfcc1d,b'\0')
word(0x1e6530c,0);word(0x1e65324,1)
word(0x1cfcaf4,272);word(0x17c7c4c,0);word(0x5a7df8,0);u.mem_write(0x17c7c20,b'\0')
for lod in (0,0xbf400000,0x3f800000):
    calls.clear();word(0x5aa7f0,lod);word(0x1e64da0,0xffffffff)
    u.mem_write(stack,struct.pack('<II',stop,0x18000));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x54f160,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    assert calls==[(32,(0,19,lod)),(48,(0,0)),(48,(1,0)),
        (32,(0,1,2)),(32,(0,2,0)),(32,(0,4,2)),(32,(0,5,0)),(32,(1,1,1)),
        (16,(27,1)),(16,(19,5)),(16,(20,6)),(16,(15,0)),(16,(7,0)),(16,(14,0)),(16,(28,0))],calls
    assert bytes(u.mem_read(0x5aa7e4,2))==b'\1\1'
report=dict(result='PASS',cases=3,original_sha256=digest,mode=0x18000,calls=calls,
 scope='Complete54f160 with empty batch, prepared texture-stage indices and supported blend caps; real negative-handle unbind helpers. Fake device records texture0/1 detach, diffuse color/alpha selection, SRCALPHA/INVSRCALPHA, depth/alpha-test/fog off. No original GPU pixels.')
(root/'artifacts/flash-render-state.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))

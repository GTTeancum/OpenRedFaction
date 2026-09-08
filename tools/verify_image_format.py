"""Compare format classification against original instruction blocks."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
stack=0x30000000;u.mem_map(stack,65536);esp=stack+60000;output=stack+65000;stop=stack+65400
values=list(range(256))+[0x7fffffff,0x80000000,0xffffffff]
for value in values:
    u.mem_write(esp+0x10,struct.pack('<I',value));u.mem_write(esp+0x4d8,struct.pack('<I',output));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x50fe39,0x50fe9d,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x50fe9d
    fmt,=struct.unpack('<I',bytes(u.mem_read(output,4)))
    u.mem_write(esp,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EAX,value)
    u.emu_start(0x51071d,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop
    alpha=u.reg_read(UC_X86_REG_EAX)
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_image_probe.exe'),'--format',str(value)],text=True)
    assert list(map(int,actual.split()))==[fmt,alpha],value
report=dict(result='PASS',cases=len(values),scope='Unchanged original TGA depth dispatch and transparency block after format lookup; header IO and animated handle resolution excluded')
(root/'artifacts/image-format-verification.json').write_text(json.dumps(report,indent=2));print(report)

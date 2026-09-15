"""Run original466bd9..466c1d material setup and all three actual setters.

Only texture loading is supplied. Level material handle and blast flags are
inputs; this does not validate the loader, CSG or final rendered appearance.
"""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_EAX
from extract_geomod_template import SHA

def main():
    exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    base=0x30000000;stack=base+0xe000;u.mem_map(base,65536)
    pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
    read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
    loads=[]
    def hook(cpu,address,size,data):
        if address!=0x50f6a0:return
        sp=cpu.reg_read(UC_X86_REG_ESP)
        name=bytes(cpu.mem_read(read(sp+4),64)).split(b'\0')[0]
        assert name==b'ice_ice01.tga' and read(sp+8)==0xffffffff and read(sp+12)==1
        loads.append(name.decode());cpu.reg_write(UC_X86_REG_EAX,202)
        cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
    u.hook_add(UC_HOOK_CODE,hook);rows=[]
    for flags in range(256):
        loads.clear();u.mem_write(base,bytes(128));u.mem_write(base+0x38,pack(flags))
        u.mem_write(0x646000,pack(101));u.mem_write(0x5a3ea0,pack(0,0,0))
        u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
        u.emu_start(0x466bd9,0x466c1d,count=1000)
        assert u.reg_read(UC_X86_REG_EIP)==0x466c1d
        selected=struct.unpack('<3I',u.mem_read(0x5a3ea0,12));expected=202 if flags&16 else 101
        assert selected==(expected,)*3 and len(loads)==bool(flags&16)
        rows.append(dict(flags=flags,materials=selected,loads=list(loads)))
    (ROOT/'artifacts/geomod-material-setup-original.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=SHA,cases=rows),indent=2)+'\n')
    print('PASS: 256 original blast flag cases; all orientations share level/ice material')

if __name__=='__main__':main()

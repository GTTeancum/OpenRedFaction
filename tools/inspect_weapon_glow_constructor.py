"""Execute original rocket point-light call setup, stopping before allocation.

This proves constructor arguments and the enabled gate, not projectile movement,
resource lifetime, rendering, or the table loader's normalization.
"""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP
from extract_geomod_template import SHA

def main():
    data=(ROOT/'Installed_Game/RF.exe').read_bytes()
    assert hashlib.sha256(data).hexdigest()==SHA
    image=pefile.PE(data=data).get_memory_mapped_image()
    cpu=Uc(UC_ARCH_X86,UC_MODE_32)
    cpu.mem_map(0x400000,(len(image)+4095)&~4095);cpu.mem_write(0x400000,image)
    base=0x30000000;cpu.mem_map(base,65536)
    obj,info,pos,stack=base,base+0x1000,base+0x2000,base+0xe000
    reached=[]
    def stop(uc,address,size,context):
        if address in (0x4d8ed0,0x4c7b81):
            reached.append(address);uc.emu_stop()
    cpu.hook_add(UC_HOOK_CODE,stop)
    rows=[]
    for inner,enabled in [(1,1),(2,1),(1,.5),(1,0),(1,-1)]:
        reached.clear();cpu.mem_write(base,bytes(65536));cpu.mem_write(0x64ecbb,b'\x00')
        cpu.mem_write(obj+0x294,struct.pack('<I',info))
        cpu.mem_write(info+0x214,struct.pack('<6f',inner,3,enabled,100/255,50/255,100/255))
        cpu.mem_write(pos,struct.pack('<3f',1,2,3))
        cpu.reg_write(UC_X86_REG_ESI,obj);cpu.reg_write(UC_X86_REG_EDI,pos);cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.emu_start(0x4c7b23,0x4c7b82,count=100)
        row=dict(inner=inner,enabled=enabled,called=enabled>0)
        assert reached==[0x4d8ed0 if enabled>0 else 0x4c7b81],reached
        if enabled>0:
            args=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP)+4,36))
            position,radius,intensity,r,g,b,dynamic,shadow,profile=struct.unpack('<I5f3I',args)
            assert position==pos and radius==3 and intensity==enabled and (dynamic,shadow,profile)==(1,1,0)
            assert struct.pack('<3f',r,g,b)==struct.pack('<3f',100/255,50/255,100/255)
            row.update(radius=radius,intensity=intensity,color=[r,g,b],dynamic=dynamic,shadow=shadow,profile=profile)
        rows.append(row)
    out=ROOT/'artifacts/projectile-glow';out.mkdir(parents=True,exist_ok=True)
    (out/'original-constructor.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=SHA,cases=rows),indent=2)+'\n')
    print('PASS: original rocket light constructor arguments, inner-radius independence and enabled gate')

if __name__=='__main__':main()

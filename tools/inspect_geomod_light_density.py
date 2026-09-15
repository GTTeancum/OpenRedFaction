"""Execute original4e43eb..4e443a without hooks against shared density scaling.

The grouped maximum detail class is supplied. Generated-face flag inheritance,
grouping and later lightmap relighting are not established by this oracle.
"""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_FPCW
from extract_geomod_template import SHA

def main():
    exe=ROOT/'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    image=pefile.PE(str(exe)).get_memory_mapped_image()
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    base=0x30000000;stack=base+0xe000;u.mem_map(base,65536)
    rows=[];probe=str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe')
    for detail in range(4):
        for x in (.00001,.1,.25,.7,1.,2.,4.,15.125,64.,1024.):
            values=struct.unpack('<2f',struct.pack('<2f',x,x*1.73))
            u.mem_write(base,bytes(128));u.mem_write(base+0x2c,struct.pack('<2f',*values))
            u.mem_write(stack,bytes(128));u.mem_write(stack+0x30,struct.pack('<I',detail))
            u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
            u.emu_start(0x4e43eb,0x4e443a,count=1000)
            assert u.reg_read(UC_X86_REG_EIP)==0x4e443a
            expected=bytes(u.mem_read(base+0x2c,8))
            actual=subprocess.check_output([probe,'--light-density',*map(str,values),str(detail)],text=True).split()
            assert struct.pack('<2f',*map(float,actual))==expected,(detail,values,actual)
            rows.append(dict(detail=detail,input=values,output=struct.unpack('<2f',expected)))
    (ROOT/'artifacts/geomod-light-density-original.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=SHA,cases=rows),indent=2)+'\n')
    print('PASS:',len(rows),'unhooked original/shared bit-exact density cases')

if __name__=='__main__':main()

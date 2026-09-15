"""Unhooked original4e4452..4e453e rounded lightmap extents vs shared C.

Inputs are already measured spans and detail-adjusted densities. Allocation,
dominant-axis selection, face grouping and detail ownership are excluded.
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
    exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    pe=pefile.PE(str(exe));image=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    base=0x30000000;stack=base+0xe000;u.mem_map(base,65536)
    floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
    f32=lambda v:struct.unpack('<f',floats(v))[0]
    rows=[]
    for density in (.25,2.,4.,16.):
      for span in (0.,.0001,.12499999,.125,.12500001,.875,1.,1.1249999,1.125,3.25,15.875,16.,16.125,32.,1000.):
       for special in (0,1):
        spans=[f32(span),f32(span*.73)];densities=[density,density*1.5]
        u.mem_write(base,bytes(128));u.mem_write(base+9,bytes([special]));u.mem_write(base+0x2c,floats(*densities))
        u.mem_write(stack,bytes(128));u.mem_write(stack+0x28,floats(*spans))
        u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
        u.emu_start(0x4e4452,0x4e453e,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4e453e
        sizes=struct.unpack('<2I',u.mem_read(base+0x18,8));adjusted=bytes(u.mem_read(base+0x2c,8))
        values=subprocess.check_output([str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),'--light-size',*map(str,spans),*map(str,densities),str(special)],text=True).split()
        assert tuple(map(int,values[:2]))==sizes
        assert floats(*map(float,values[2:]))==adjusted,(spans,densities,special,values,struct.unpack('<2f',adjusted))
        rows.append(dict(span=spans,density=densities,special=special,dimensions=sizes,adjusted_density=struct.unpack('<2f',adjusted)))
    (ROOT/'artifacts/geomod-light-size-original.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=SHA,cases=rows),indent=2)+'\n')
    print('PASS:',len(rows),'original/shared lightmap sizes and bit-exact adjusted densities')

if __name__=='__main__':main()

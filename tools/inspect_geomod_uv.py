"""Execute original mode4 UV mapping; supply only bitmap dimensions.

Synthetic original face/corner records exercise axis signs, ties, dimensions,
negative positions and corner list traversal. No original renderer is hooked.
"""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from extract_geomod_template import SHA

def run():
    exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    pe=pefile.PE(str(exe));im=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(im)+4095)&~4095);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,im)
    base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
    w=lambda *x:struct.pack('<'+'I'*len(x),*x)
    f=lambda *x:struct.pack('<'+'f'*len(x),*x)
    read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
    width=height=calls=0
    def hook(cpu,address,size,_):
        nonlocal calls
        if address!=0x510630:return
        sp=cpu.reg_read(UC_X86_REG_ESP);assert read(sp+4)==101
        cpu.mem_write(read(sp+8),w(width));cpu.mem_write(read(sp+12),w(height));calls+=1
        cpu.reg_write(UC_X86_REG_EAX,0);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
    u.hook_add(UC_HOOK_CODE,hook)
    u.mem_write(0x5a3e9c,w(4,101,101,101));u.mem_write(0x5a3eac,f(32))
    normals=[(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1),
             (1,1,0),(-1,-1,0),(1,1,1),(-1,-1,-1),(1,0,1),(0,1,1),(.2,-.7,.3),(-.9,.1,.3)]
    positions=[(13.25,-4.75,8.125),(-16,-12,20),(0,0,0)]
    rows=[]
    for width,height in [(256,256),(128,64),(73,91)]:
        for normal in normals:
            calls=0;u.mem_write(base,bytes(128));u.mem_write(base,f(*normal));u.mem_write(base+0x40,w(base+0x1000))
            for i,p in enumerate(positions):
                node=base+0x1000+i*32;point=base+0x2000+i*16
                u.mem_write(point,f(*p));u.mem_write(node,w(point));u.mem_write(node+0x14,w(base+0x1000+((i+1)%3)*32))
            u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
            u.emu_start(0x4f8740,stop,count=100000)
            assert u.reg_read(UC_X86_REG_EIP)==stop and calls==1 and read(base+0x30)==101
            for i,p in enumerate(positions):
                original=bytes(u.mem_read(base+0x1004+i*32,8))
                args=[str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),'--uv',str(width),str(height),*map(str,normal),*map(str,p)]
                values=list(map(float,subprocess.check_output(args,text=True).split()))
                assert f(*values)==original,(normal,width,height,p,values,struct.unpack('<2f',original))
                rows.append(dict(normal=normal,width=width,height=height,position=p,uv=values))
    (ROOT/'artifacts/geomod-uv-original.json').write_text(json.dumps(dict(exe_sha256=SHA,scope=__doc__,bit_exact=True,cases=rows),indent=2)+'\n')
    print('PASS:',len(rows),'bit-exact original planar UV pairs; signs, ties, non-square dimensions and ring traversal')

if __name__=='__main__':run()

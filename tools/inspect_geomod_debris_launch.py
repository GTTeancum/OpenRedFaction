"""Compare original490150 debris launch with shared C; only CRT draws supplied.

Original normalization, cone rotation and speed branches execute unmodified.
This does not establish the live RNG stream, spawn placement or later physics.
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
    pe=pefile.PE(str(exe));image=pe.get_memory_mapped_image();cpu=Uc(UC_ARCH_X86,UC_MODE_32)
    start=pe.OPTIONAL_HEADER.ImageBase;cpu.mem_map(start,(len(image)+4095)&~4095);cpu.mem_write(start,image)
    base=0x30000000;cpu.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000;origin_ptr=base+0x1000
    pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
    floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
    f32=lambda v:struct.unpack('<f',floats(v))[0]
    read=lambda a:struct.unpack('<I',cpu.mem_read(a,4))[0]
    state=draws=0
    def hook(u,address,size,_):
        nonlocal state,draws
        if address!=0x57312d:return
        state=(state*214013+2531011)&0xffffffff;draws+=1;sp=u.reg_read(UC_X86_REG_ESP)
        u.reg_write(UC_X86_REG_EAX,(state>>16)&32767);u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
    cpu.hook_add(UC_HOOK_CODE,hook)
    threshold=struct.unpack('<I',floats(.13))[0]
    radii=[.05,*[struct.unpack('<f',pack(threshold+i))[0] for i in (-1,0,1)],.15,.25]
    points=[(0,0,0),(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1),(.2,-.7,.3),(1e-8,2,1e-8)]
    rows=[]
    for seed in (0,1,0xffffffff,7654321):
     for radius in radii:
      for position in points:
       for origin in ((0,0,0),(-16,-12,20)):
        radius=f32(radius);resistance=f32((radius-f32(.05))*5);state=seed;draws=0
        cpu.mem_write(base,bytes(128));cpu.mem_write(base+8,floats(*position));cpu.mem_write(origin_ptr,floats(*origin))
        cpu.mem_write(base+0x38,floats(radius,resistance));cpu.mem_write(stack,pack(stop,base,origin_ptr))
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x37f);cpu.emu_start(0x490150,stop,count=100000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop and draws==2
        original=bytes(cpu.mem_read(base+0x48,12))
        args=[str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),'--debris-launch',str(seed),str(radius),str(resistance),*map(str,position),*map(str,origin)]
        values=subprocess.check_output(args,text=True).split();assert int(values[0])==state
        shared=list(map(float,values[1:]));assert floats(*shared)==original,(seed,radius,position,origin,shared,struct.unpack('<3f',original))
        rows.append(dict(seed=seed,radius=radius,resistance=resistance,position=position,origin=origin,velocity=shared,state=state))
    (ROOT/'artifacts/geomod-debris-launch-original.json').write_text(json.dumps(dict(exe_sha256=SHA,entry='00490150',scope=__doc__,bit_exact=True,cases=rows),indent=2)+'\n')
    print('PASS:',len(rows),'bit-exact original/shared debris launches, including .13 branch neighbors and zero separation')
if __name__=='__main__':run()

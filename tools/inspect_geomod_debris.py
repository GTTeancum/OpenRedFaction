"""Execute original490230 debris mesh construction with supplied CRT/bitmap boundaries.

Captures geometry and UVs; does not establish spawning, physics or live fidelity.
Original instructions perform all geometry and projection calculations.
"""
import hashlib, json, struct, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from extract_geomod_template import SHA

def run():
    exe=ROOT/'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    pe=pefile.PE(str(exe)); image=pe.get_memory_mapped_image()
    cpu=Uc(UC_ARCH_X86,UC_MODE_32); start=pe.OPTIONAL_HEADER.ImageBase
    cpu.mem_map(start,(len(image)+4095)&~4095);cpu.mem_write(start,image)
    base=0x30000000;cpu.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
    pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
    read=lambda a:struct.unpack('<I',cpu.mem_read(a,4))[0]
    state=draws=bitmap_calls=0;width=height=0
    def hook(u,address,size,_):
        nonlocal state,draws,bitmap_calls
        if address not in (0x57312d,0x510630):return
        sp=u.reg_read(UC_X86_REG_ESP)
        if address==0x57312d:
            state=(state*214013+2531011)&0xffffffff;draws+=1
            u.reg_write(UC_X86_REG_EAX,(state>>16)&32767)
        else:
            assert read(sp+4)==101
            u.mem_write(read(sp+8),pack(width));u.mem_write(read(sp+12),pack(height))
            bitmap_calls+=1;u.reg_write(UC_X86_REG_EAX,0)
        u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
    cpu.hook_add(UC_HOOK_CODE,hook)
    rows=[]
    for seed in (0,1,0xffffffff,1234567,7654321):
      for radius in (.05,.13,.15,.25):
       for width,height in ((256,256),(128,64),(73,91)):
        state=seed;draws=bitmap_calls=0
        cpu.mem_write(base,bytes(1024));cpu.mem_write(base+0x38,struct.pack('<f',radius));cpu.mem_write(base+0x44,pack(101))
        cpu.mem_write(stack,pack(stop,base));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x37f)
        cpu.emu_start(0x490230,stop,count=100000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop and draws==25 and bitmap_calls==1
        vertices=[list(struct.unpack('<3f',cpu.mem_read(base+0x7c+i*12,12))) for i in range(8)]
        faces=[];uv=[]
        for i in range(12):
            corners=[];coords=[]
            for j in range(3):
                a=base+0xdc+i*36+j*12;ptr=read(a+8)
                index,rem=divmod(ptr-(base+0x7c),12)
                assert rem==0 and 0<=index<8
                corners.append(index);coords.append(list(struct.unpack('<2f',cpu.mem_read(a,8))))
            faces.append(corners);uv.append(coords)
        assert len({tuple(sorted(f)) for f in faces})==12
        rows.append(dict(seed=seed,state=state,radius=radius,width=width,height=height,vertices=vertices,faces=faces,uv=uv,lifetime=struct.unpack('<f',cpu.mem_read(base+0x74,4))[0]))
    for row in rows:
        args=[str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),'--debris',str(row['seed']),str(row['radius']),str(row['width']),str(row['height'])]
        values=subprocess.check_output(args,text=True).split()
        assert int(values[0])==row['state']
        shared=list(map(float,values[1:]))
        original=[row['lifetime']]+sum(row['vertices'],[])+sum(row['faces'],[])+[x for face in row['uv'] for corner in face for x in corner]
        assert len(shared)==len(original)
        assert struct.pack('<'+'f'*len(shared),*shared)==struct.pack('<'+'f'*len(original),*original),row
    report=dict(exe_sha256=SHA,entry='00490230',scope=__doc__,bit_exact=True,cases=rows)
    (ROOT/'artifacts/geomod-debris-original.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(rows),'bit-exact original/shared debris meshes; 8 vertices,12 triangles,25 RNG draws each')
if __name__=='__main__':run()

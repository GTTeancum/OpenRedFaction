"""Compare ordinary region selection and scale with original 45cff0 execution.

Only the region-list container is supplied. Original sphere/box membership,
overlap policy, ice flag and hardness arithmetic execute unmodified. Shallow
regions are outside this fixture's scope.
"""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
from extract_geomod_template import SHA

def region(hardness=25,flags=4,position=(0,0,0),basis=(0,0,1,1,0,0,0,1,0),dimensions=(56,48,60),radius=3):
    return dict(hardness=hardness,flags=flags,position=position,basis=basis,dimensions=dimensions,radius=radius)

def run():
    exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    pe=pefile.PE(str(exe));im=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(im)+4095)&~4095);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,im)
    base=0x30000000;u.mem_map(base,0x100000);stack=base+0xe0000;stop=base+0xf0000
    w=lambda *x:struct.pack('<'+'I'*len(x),*x)
    f=lambda *x:struct.pack('<'+'f'*len(x),*x)
    read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
    regions=[]
    def hook(cpu,address,size,_):
        if address not in (0x40a490,0x40a480):return
        assert cpu.reg_read(UC_X86_REG_ECX)==0x6460a4
        sp=cpu.reg_read(UC_X86_REG_ESP)
        value=len(regions) if address==0x40a490 else base+0x2000+read(sp+4)*4
        cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,read(sp))
        cpu.reg_write(UC_X86_REG_ESP,sp+(4 if address==0x40a490 else 8))
    u.hook_add(UC_HOOK_CODE,hook)
    u.mem_write(0x1754474,w(7))
    cases=[]
    for hardness in (0,25,55,99,100):
        for scale in (.125,1,5/0.2196311503648758,123.75):
            cases.append(([region(hardness)],0,(0,0,0),scale))
    for default in (0,25,55,99,100):cases.append(([],default,(0,0,0),5))
    for p in ((28,0,0),(28.0001,0,0),(-28,24,30),(0,-24,0),(0,0,-30),(0,0,30.0001)):
        cases.append(([region()],0,p,5))
    for p in ((3,0,0),(2.9999,0,0),(0,-3,0),(0,0,3.0001),(1,1,1)):
        cases.append(([region(flags=2)],0,p,5))
    for p in ((4,2,3),(1,4,3),(1,2,4),(1,2,3)):
        cases.append(([region(position=(1,2,3),basis=(1,0,0,0,0,-1,0,1,0),dimensions=(2,4,6))],0,p,5))
    for regs in ([region(25),region(75)], [region(75),region(25)],
                 [region(25),region(75,position=(100,0,0))],
                 [region(25,flags=68),region(75)], [region(100,flags=66)]):
        cases.append((regs,0,(0,0,0),5))
    rows=[]
    for regions,default,point,scale in cases:
        u.mem_write(base,bytes(0x4000));u.mem_write(stack-0x1000,bytes(0x1000))
        u.mem_write(base,f(*point));u.mem_write(base+0x103c,f(scale));u.mem_write(0x646004,w(default or 55))
        for i,r in enumerate(regions):
            address=base+0x3000+i*0x50;u.mem_write(base+0x2000+i*4,w(address))
            u.mem_write(address,w(r['flags']&7,r['hardness']))
            u.mem_write(address+8,bytes((bool(r['flags']&32),bool(r['flags']&64))))
            u.mem_write(address+0x10,f(*r['position']))
            b=r['basis'];u.mem_write(address+0x1c,f(*b[3:6],*b[6:9],*b[:3]))
            u.mem_write(address+0x40,f(r['radius'],*r['dimensions']))
        u.mem_write(stack,w(stop,base,base+0x1000,1));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
        u.emu_start(0x45cff0,stop,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==stop
        allowed=u.reg_read(UC_X86_REG_EAX)&255;flags=read(base+0x1038);scaled=bytes(u.mem_read(base+0x103c,4))
        lines=[' '.join(map(str,(len(regions),default,scale,*point)))]
        for r in regions:lines.append(' '.join(map(str,(r['flags'],r['hardness'],*r['position'],*r['basis'],*r['dimensions'],r['radius']))))
        result=subprocess.check_output([str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),'--hardness'],input='\n'.join(lines)+'\n',text=True).split()
        assert int(result[1])==allowed and int(result[3])==flags and f(float(result[4]))==scaled,(lines,result,allowed,flags,struct.unpack('<f',scaled))
        rows.append(dict(regions=regions,default=default,point=point,scale=scale,result=result))
    (ROOT/'artifacts/geomod-hardness-original.json').write_text(json.dumps(dict(exe_sha256=SHA,scope=__doc__,bit_exact=True,cases=rows),indent=2)+'\n')
    print('PASS:',len(rows),'original ordinary-region cases: boundaries, rotated box, overlap, fallback, ice, refusal and bit-exact scale')

if __name__=='__main__':run()

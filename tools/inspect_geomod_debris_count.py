"""Execute original490500/490890 count policy with synthetic collision results.

Only the48fc10 world-query boundary is supplied; probe construction, surface
flag rejection, accumulation, rounding and count cap execute original code.
This does not validate actual world intersection or material ownership.
"""
import hashlib,json,re,struct,subprocess,sys
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
    base=0x30000000;cpu.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
    pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
    floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
    read=lambda a:struct.unpack('<I',cpu.mem_read(a,4))[0]
    native=pefile.PE(str(ROOT/'build/xbox/main.exe'));native_image=native.get_memory_mapped_image()
    xbox=Uc(UC_ARCH_X86,UC_MODE_32);native_base=native.OPTIONAL_HEADER.ImageBase
    xbox.mem_map(native_base,(len(native_image)+4095)&~4095);xbox.mem_write(native_base,native_image);xbox.mem_map(base,0x10000)
    native_entry=int(re.search(r'_rf_geomod_debris_count\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
    probes=[];cases=[];radius=0;case=None
    def hook(u,address,size,_):
        if address!=0x48fc10:return
        sp=u.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+i*4)
        assert arg(4)==0 and read(arg(2))==99 and len(probes)<14
        i=len(probes);probes.append([list(struct.unpack('<3f',u.mem_read(arg(j),12))) for j in (0,1)])
        mode=case[i];face=base+0x2000
        hit=mode[0];u.mem_write(face+0x28,pack(mode[2]))
        u.mem_write(arg(3),pack(face if mode[1] else 0)+floats(mode[3]))
        u.reg_write(UC_X86_REG_EAX,hit);u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
    cpu.hook_add(UC_HOOK_CODE,hook)
    for radius in (.1,.125,.5,1,3.75,5):
     configs=[(label,[entry]*14) for label,entry in [
        ('miss',(0,0,0,0)),('near',(1,1,0,.25)),('far',(1,1,0,1)),
        ('flag8',(1,1,8,0)),('no_face',(1,0,0,0)),('zero',(1,1,0,0)),
        ('hit2',(2,1,0,0)),('other_flags',(1,1,0xfffffff7,0))]]
     for selected in range(14):
        entries=[(0,0,0,0)]*14;entries[selected]=(1,1,0,.33)
        configs.append(('only_'+str(selected),entries))
     configs.append(('mixed',[(i%3,i%2,(i%4)*4,(i%5)/4) for i in range(14)]))
     for label,case in configs:
        probes=[];cpu.mem_write(base,floats(-16,-12,20))
        cpu.mem_write(stack,pack(stop,base)+floats(radius)+pack(99));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x37f)
        cpu.emu_start(0x490500,stop,count=100000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop and len(probes)==14
        count=struct.unpack('<i',pack(cpu.reg_read(UC_X86_REG_EAX)))[0]
        data=' '.join(map(str,[radius,-16,-12,20]))+'\n'+'\n'.join(' '.join(map(str,p)) for p in case)+'\n'
        values=subprocess.check_output([str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),'--debris-count'],input=data,text=True).split()
        assert int(values[0])==count,(radius,label,values[0],count)
        xbox.mem_write(base,b''.join(struct.pack('<3If',*probe) for probe in case));xbox.mem_write(base+0x400,pack(0xa5a5a5a5))
        xbox.mem_write(stack,pack(stop)+floats(radius)+pack(base,base+0x400));xbox.reg_write(UC_X86_REG_ESP,stack);xbox.reg_write(UC_X86_REG_FPCW,0x37f)
        xbox.emu_start(native_entry,stop,count=10000)
        assert xbox.reg_read(UC_X86_REG_EIP)==stop and xbox.reg_read(UC_X86_REG_EAX)==0
        assert struct.unpack('<i',xbox.mem_read(base+0x400,4))[0]==count,(radius,label,'NXDK')

        endpoints=[x for probe in probes for x in probe[1]]
        assert floats(*map(float,values[1:]))==floats(*endpoints),(radius,label)
        cases.append(dict(radius=radius,label=label,inputs=case,probes=probes,count=count))
    (ROOT/'artifacts/geomod-debris-count-original.json').write_text(json.dumps(dict(exe_sha256=SHA,entry='00490500',scope=__doc__,bit_exact=True,cases=cases),indent=2)+'\n')
    print('PASS:',len(cases),'original/PC/NXDK debris counts and14 probe endpoints; flags, misses, rounding and cap')
if __name__=='__main__':run()

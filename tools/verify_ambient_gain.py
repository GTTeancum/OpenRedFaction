"""Unhooked original sample/ambient gains versus PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stub=base+0xf000;stop=stub+6
def machine(path):
    p=pefile.PE(str(path));image=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,image)
    m.mem_map(base,65536);m.mem_write(stub,b'\xd9\x1d'+w(base+0x3000));m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
mapping=(root/'build/xbox/main.map').read_text()
entries=[int(re.search('_rf_audio_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('sample_gain','ambient_gain')]
def call(m,address,args):
    m.mem_write(stack,w(stub)+args);m.reg_write(UC_X86_REG_ESP,stack)
    m.emu_start(address,stop,count=10000)
    assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+4
    return bytes(m.mem_read(base+0x3000,4))
rng=random.Random(0x543c20);commands=bytearray();expected=bytearray();bypass=disabled=0
for case in range(4096):
    near=(.1,1.,5.,10.)[case%4];far=near*20
    volume=(-1.,0.,.25,.8,1.,2.)[case//4%6];factor=(-1.,0.,.25,1.,2.)[case//24%5]
    scale=(-1.,0.,.25,1.,2.)[case//120%5];category=(-.5,0.,.5,1.,2.)[case//600%5]
    enabled=(0,1,255,256,257)[case//7%5]
    listener=[0.,0.,0.];position=[0.,0.,0.]
    mode=case%8
    if mode==1:position[0]=near
    elif mode==2:position[0]=far
    elif mode==3:position[0]=far+.01
    elif mode==4:position[0]=near*2  # zero denominator for factor=-1
    elif mode>=5:
        listener=[rng.uniform(-20,20) for _ in range(3)]
        position=[v+rng.uniform(-30,30) for v in listener]
    parameters=f(near,far,volume,factor);pos=f(*position);listen=f(*listener);cat=f(category);s=f(scale)
    u.mem_write(0x1cd3bc8,f(volume,near,far,factor));u.mem_write(0x1cd3bdc,w(0));u.mem_write(0x1cd3b94,cat)
    u.mem_write(0x1cd3b88,listen);u.mem_write(0x1cfc5d0,bytes([enabled&255]));u.mem_write(base+0x1000,pos)
    original=call(u,0x543a60,w(0)+s)+call(u,0x543c20,w(0,base+0x1000)+s)
    x.mem_write(base,parameters);x.mem_write(base+0x1000,pos);x.mem_write(base+0x1100,listen)
    actual=call(x,entries[0],f(volume)+cat+s)+call(x,entries[1],w(base,base+0x1000,base+0x1100)+cat+s+w(enabled))
    assert actual==original,(case,struct.unpack('<2f',actual),struct.unpack('<2f',original))
    commands.extend(parameters+pos+listen+cat+s+w(enabled));expected.extend(original)
    bypass+=int(pos==listen and bool(enabled&255));disabled+=int(not(enabled&255))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--ambient-gain'],input=commands)
assert actual==expected
report=dict(result='PASS',cases=4096,functions=2,equal_position_cases=bypass,disabled_cases=disabled,
            original_sha256=digest,x87_control_word='0x027f',scope='Unhooked543a60,543c20 and actual equality/distance/vector/clamp callees. '
            'Exact binary32 caller results vs PC/NXDK; category/default/scale, near/far/equality boundaries, zero attenuation denominator. No device/PCM integration.')
(root/'artifacts/ambient-gain.json').write_text(json.dumps(report,indent=2));print(report)

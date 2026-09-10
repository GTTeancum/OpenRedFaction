"""Full original 515d00 box transform/clip/project versus shared PC/NXDK."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_visibility_box_project\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(a,args=()):
    u.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(a,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
rng=random.Random(0x515d00);commands=bytearray();expected=bytearray();visible=0
for case in range(1024):
    minimum=tuple(rng.randint(-128,256)/32 for _ in range(3));maximum=tuple(v+rng.randint(0,128)/32 for v in minimum)
    origin=tuple(rng.randint(-16,16)/32 for _ in range(3))
    matrix=(1.,0.,0.,0.,1.,0.,0.,0.,1.) if case%2 else (0.8,0.,0.6,0.,1.,0.,-0.6,0.,0.8)
    perspective=0 if case%11==0 else 1;far=case%2
    view=struct.pack('<13f4IfI3f2i',*origin,*matrix,4.,perspective,1,perspective,far,8.,1,0.,320.,240.,0,0)
    assert len(view)==96
    packet=view+struct.pack('<6f',*minimum,*maximum);commands.extend(packet)
    call(0x549270)
    u.mem_write(0x17c7bcc,struct.pack('<I',0x66));u.mem_write(0x5a4d18,bytes((1,perspective)))
    u.mem_write(0x1818b65,bytes((far,)));u.mem_write(0x1818b6c,struct.pack('<f',8.));u.mem_write(0x1818b7c,struct.pack('<f',4.))
    u.mem_write(0x1818690,struct.pack('<3f',*origin));u.mem_write(0x18186c8,struct.pack('<9f',*matrix))
    u.mem_write(0x5a445a,b'\1');u.mem_write(0x1e652e8,bytes(4));u.mem_write(0x1818a5c,struct.pack('<f',320));u.mem_write(0x1818a24,struct.pack('<f',240));u.mem_write(0x17c7bec,bytes(8))
    u.mem_write(base,struct.pack('<6f',*minimum,*maximum));u.mem_write(base+0x1000,struct.pack('<4f',11,22,33,44))
    call(0x515d00,(base,base+12,base+0x1000,base+0x1004,base+0x1008,base+0x100c))
    accepted=u.reg_read(UC_X86_REG_EAX)&255;visible+=bool(accepted)
    out=struct.pack('<2I',0,accepted)+bytes(u.mem_read(base+0x1000,16));expected.extend(out)
    x.mem_write(base,packet);x.mem_write(base+0x1000,struct.pack('<I4f',0,11,22,33,44))
    x.mem_write(stack,struct.pack('<5I',stop,base,base+96,base+108,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1000,20))
    assert actual==out,(case,actual.hex(),out.hex())
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--box-project'],input=commands)==expected
report=dict(result='PASS',cases=1024,visible=visible,scope='Full original 515d00 with actual corner transform/classification, six-face clipping, projection and temporary pool versus PC/NXDK. Identity/rotated view, perspective/flat depth, far clipping, offscreen/behind and degenerate extents; exact visible flag/rectangle including preserved rejection output. View setup and native campaign rendering excluded.')
(root/'artifacts/box-projection-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Original 547150 viewport/FOV scale arithmetic versus PC/NXDK."""
import runpy,struct,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_visibility_view_scale_build\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def graphics(m,a,size,data):
    if a==0x50ce40:
        sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
        m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,graphics)
commands=bytearray();expected=bytearray();cases=0
for mode in (0,1,256,257):
 for width,height,aspect in ((640,480,1.),(1280,720,1.),(720,480,1.125),(641,479,0.9)):
  for fov in (0.5,1.99999988,2.,60.,90.,120.):
   for far in (0.,31.25,100.,999.99,1000.,2000.):
    packet=struct.pack('<4i3fI',width,height,-7,13,aspect,fov,far,mode);commands.extend(packet)
    u.mem_write(0x17c7bec,struct.pack('<4i',-7,13,width,height));u.mem_write(0x17c7bd8,struct.pack('<f',aspect));u.mem_write(0x1818b68,struct.pack('<f',far))
    u.mem_write(0x1818b65,b'\0');u.mem_write(0x1818b74,struct.pack('<f',0.1));u.mem_write(base,struct.pack('<9f',1,0,0,0,1,0,0,0,1));u.mem_write(base+64,bytes(12))
    u.mem_write(stack,struct.pack('<IIIfII',stop,base,base+64,fov,0,mode));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x547150,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
    addresses=(0x1818b48,0x1818b4c,0x1818b50,0x1818a5c,0x1818a24,0x1818b54,0x1818b5c,0x1818b7c,0x1818a60)
    out=bytes(4)+b''.join(u.mem_read(a,4) for a in addresses);expected.extend(out)
    x.mem_write(base,packet);x.mem_write(base+0x1000,bytes([0xa5])*36)
    x.mem_write(stack,struct.pack('<3I',stop,base,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1000,36))
    assert actual==out,(mode,width,fov,far,actual.hex(),out.hex())
    cases+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--visibility-view-scale'],input=commands)==expected
report=dict(result='PASS',cases=cases,scope='Original 547150 executes with actual tangent, viewport arithmetic, matrix helpers and frustum setup; only graphics-state call 50ce40 intercepted. Exact selected viewport/scale outputs versus PC/NXDK across modes, viewport/aspect, FOV threshold and far clamp. Other original side effects and live camera integration excluded.')
(root/'artifacts/visibility-view-scale-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Full original 546a40 frustum assembly versus PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_visibility_frustum_build\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x546a40);commands=bytearray();expected=bytearray();counts={}
for case in range(1024):
    origin=tuple(rng.randint(-10000,10000)/64 for _ in range(3))
    basis=((1,0,0,0,1,0,0,0,1),(0.8,0,0.6,0,1,0,-0.6,0,0.8),(0,0,-1,0,1,0,1,0,0),(1,0,0,0,0.6,0.8,0,-0.8,0.6))[case%4]
    scale=tuple(rng.randint(8,80)/16 for _ in range(3));far=rng.randint(1,1000)/8;near=rng.randint(0,8)/16
    mode=(0,1,256,257)[case//4%4];far_enabled=(0,1,256,257)[case//16%4]
    packet=struct.pack('<17f2I',*origin,*basis,*scale,far,near,mode,far_enabled);commands.extend(packet)
    u.mem_write(0x1818690,packet[:12]);u.mem_write(0x18186a0,packet[12:48]);u.mem_write(0x1818b48,packet[48:60])
    u.mem_write(0x1818b68,packet[60:64]);u.mem_write(0x1818b74,packet[64:68]);u.mem_write(0x5a4d19,bytes([mode&255]));u.mem_write(0x1818b65,bytes([far_enabled&255]))
    u.mem_write(0x1818a68,bytes([0xa5])*168)
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x546a40,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    n=struct.unpack('<I',u.mem_read(0x1818b8c,4))[0];counts[n]=counts.get(n,0)+1
    out=bytearray(4);tags=[]
    for i in range(6):
        raw=bytes(u.mem_read(0x1818a68+i*28,28));out.extend(raw[4:24]);tags.append(raw[0] if i<n else 0xa5a5a5a5)
    out.extend(struct.pack('<7I',*tags,n));out.extend(u.mem_read(0x1818b6c,4));out.extend(u.mem_read(0x1818b78,4));expected.extend(out)
    x.mem_write(base,packet);x.mem_write(base+0x1000,bytes([0xa5])*156)
    x.mem_write(stack,struct.pack('<3I',stop,base,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1000,156))
    assert actual==out,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,out)) if a!=b][:10])
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--visibility-frustum'],input=commands)==expected
report=dict(result='PASS',cases=1024,plane_counts=counts,scope='Full original 546a40 with actual vector arithmetic and plane constructors versus PC/NXDK. Translated/rotated views, varying scales and distances, flat/perspective and far low-byte flags; exact plane fields, masks/count, scaled distances and unused-slot preservation. FOV/window-to-view setup and campaign rendering excluded.')
(root/'artifacts/visibility-frustum-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

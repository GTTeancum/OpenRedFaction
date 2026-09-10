"""Original view-plane constructors and corner selectors versus PC/NXDK."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
symbols=(root/'build/xbox/main.map').read_text()
entries=[int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16) for name in ('rf_visibility_plane_normal','rf_visibility_plane_points')]
rng=random.Random(0x547b40);commands=bytearray();expected=bytearray()
for case in range(2048):
    mode=case%2;vectors=[tuple(rng.randint(-1000,1000)/64 for _ in range(3)) for _ in range(3)]
    if mode==0 and case%4==0:vectors[0]=tuple((-1.,-0.,0.,1.)[(case//4//(4**i))%4] for i in range(3))
    packet=struct.pack('<I9f',mode,*(v for xyz in vectors for v in xyz));commands.extend(packet)
    u.mem_write(base,bytes([0xa5])*28)
    args=struct.pack('<II',stop,4)+struct.pack('<'+'f'*(9 if mode else 6),*(v for xyz in vectors[:3 if mode else 2] for v in xyz))
    u.mem_write(stack,args);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
    u.emu_start(0x547b40 if mode else 0x547b90,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    out=bytes(4)+bytes(u.mem_read(base+4,20));expected.extend(out)
    x.mem_write(base,packet);x.mem_write(base+0x1000,bytes([0xa5])*20)
    args=(base+4,base+16,base+28,base+0x1000) if mode else (base+4,base+16,base+0x1000)
    x.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entries[mode],stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1000,20))
    assert actual==out,(case,actual.hex(),out.hex())
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--visibility-plane'],input=commands)==expected
report=dict(result='PASS',cases=2048,scope='Original 547b40/547b90 with actual cross, normalize, signed distance and 5398a0 corner selector versus PC/NXDK. Normal-point and nondegenerate three-point planes, all normal sign/zero combinations. Exact normal/distance/corner; assembling complete view frustum remains excluded.')
(root/'artifacts/visibility-planes-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

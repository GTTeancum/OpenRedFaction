"""Original expanded-box and clip-plane decisions versus PC/NXDK."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_visibility_portal_classify\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(address,args):
    u.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(address,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    return u.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(0x518750);commands=bytearray();expected=bytearray();counts=[0,0,0]
for case in range(2048):
    minimum=tuple(rng.randint(-1000,1000)/32 for _ in range(3))
    maximum=tuple(v+rng.randint(0,100)/32 for v in minimum)
    camera=list(minimum)
    axis=case%3
    if case%4==0:camera[axis]=minimum[axis]-1
    elif case%4==1:camera[axis]=maximum[axis]+1
    else:camera[axis]=minimum[axis]-1.0001
    n=case%9;planes=[]
    for i in range(8):
        normal=tuple(rng.randint(-100,100)/16 for _ in range(3));distance=rng.randint(-1000,1000)/16
        if case%17==0:normal=(0.,0.,0.);distance=0.
        planes.append(struct.pack('<4fI',*normal,distance,(case+i)%8))
    packet=struct.pack('<9fI',*camera,*minimum,*maximum,n)+b''.join(planes);commands.extend(packet)
    u.mem_write(base,packet)
    if call(0x507ba0,(base,0x3f800000,base+12,base+24)):action=1
    else:
        u.mem_write(0x1818b8c,struct.pack('<I',n))
        for i,p in enumerate(planes):u.mem_write(0x1818a6c+i*28,p+bytes(8))
        action=2 if call(0x518750,(base+12,base+24)) else 0
    counts[action]+=1;out=struct.pack('<2I',0,action);expected.extend(out)
    x.mem_write(base,packet);x.mem_write(base+0x1000,bytes([0xa5])*4)
    x.mem_write(stack,struct.pack('<7I',stop,base,base+12,base+24,base+40,n,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1000,4))==out,case
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--portal-classify'],input=commands)==expected
report=dict(result='PASS',cases=2048,project=counts[0],full_view=counts[1],rejected=counts[2],scope='Original 507ba0/518750 and actual expansion/corner/plane callees versus PC/NXDK. Inclusive expanded boundaries, just-outside cases, 0..8 supplied planes, eight corner selectors and zero plane distance. View-plane generation and projected rectangles excluded.')
(root/'artifacts/portal-classify-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

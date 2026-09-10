"""Composed camera setup and box projection versus original view setup."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_visibility_view_scale.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
symbols=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'_rf_visibility_camera_setup\s+([0-9a-fA-F]+)',symbols)[1],16)
box_entry=int(re.search(r'_rf_visibility_box_project\s+([0-9a-fA-F]+)',symbols)[1],16)
rng=random.Random(547150);commands=bytearray();expected=bytearray();boxes=bytearray();box_expected=bytearray();visible=0
def call(a,args):
    u.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(a,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
for case in range(256):
    width,height=(640,480) if case%2 else (1280,720);aspect=1.;fov=(60.,90.,120.,0.75)[case%4];far=100.;mode=0 if case%7==0 else 1
    origin=tuple(rng.randint(-1000,1000)/32 for _ in range(3));basis=(1,0,0,0,1,0,0,0,1) if case%3 else (0.8,0,0.6,0,1,0,-0.6,0,0.8)
    near=.1;far_enabled=case%2;clamp=1;offset=0.
    packet=struct.pack('<4i3fI',width,height,-7,13,aspect,fov,far,mode)+struct.pack('<13f3If',*origin,*basis,near,1,far_enabled,clamp,offset)
    assert len(packet)==100;commands.extend(packet)
    u.mem_write(0x17c7bec,struct.pack('<4i',-7,13,width,height));u.mem_write(0x17c7bd8,struct.pack('<f',aspect));u.mem_write(0x1818b68,struct.pack('<f',far));u.mem_write(0x1818b74,struct.pack('<f',near))
    u.mem_write(0x1818b65,bytes([far_enabled]));u.mem_write(0x5a4d18,b'\1');u.mem_write(0x5a445a,b'\1');u.mem_write(0x1e652e8,bytes(4));u.mem_write(0x17c7bcc,struct.pack('<I',0x66))
    u.mem_write(base,struct.pack('<9f',*basis));u.mem_write(base+64,struct.pack('<3f',*origin));u.mem_write(0x1818a68,bytes([0xa5])*168)
    call(0x547150,(base,base+64,struct.unpack('<I',struct.pack('<f',fov))[0],0,mode))
    view=bytes(u.mem_read(0x1818690,12))+bytes(u.mem_read(0x18186a0,36))+bytes(u.mem_read(0x1818b48,12))+struct.pack('<2f2I',far,near,mode,far_enabled)
    n=struct.unpack('<I',u.mem_read(0x1818b8c,4))[0];planes=bytearray();tags=[]
    for i in range(6):
        raw=bytes(u.mem_read(0x1818a68+i*28,28));planes.extend(raw[4:24]);tags.append(raw[0] if i<n else 0xa5a5a5a5)
    frustum=planes+struct.pack('<7I',*tags,n)+u.mem_read(0x1818b6c,4)+u.mem_read(0x1818b78,4)
    projection=bytes(u.mem_read(0x1818690,12))+bytes(u.mem_read(0x18186c8,36))+bytes(u.mem_read(0x1818b7c,4))+struct.pack('<4I',mode,1,mode,far_enabled)+bytes(u.mem_read(0x1818b6c,4))+struct.pack('<If',clamp,offset)+u.mem_read(0x1818a5c,4)+u.mem_read(0x1818a24,4)+struct.pack('<2i',-7,13)
    out=bytes(4)+view+frustum+projection;assert len(out)==332;expected.extend(out)
    x.mem_write(base,packet);x.mem_write(base+0x1000,bytes([0xa5])*328);x.mem_write(stack,struct.pack('<3I',stop,base,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1000,328))==out,case
    minimum=tuple(origin[i]+basis[6+i]*5-1 for i in range(3));maximum=tuple(v+2 for v in minimum);bounds=struct.pack('<6f',*minimum,*maximum)
    u.mem_write(base+0x2000,bounds);u.mem_write(base+0x3000,struct.pack('<4f',11,22,33,44));call(0x515d00,(base+0x2000,base+0x200c,base+0x3000,base+0x3004,base+0x3008,base+0x300c))
    accepted=u.reg_read(UC_X86_REG_EAX)&255;visible+=bool(accepted);result=struct.pack('<2I',0,accepted)+bytes(u.mem_read(base+0x3000,16));box_expected.extend(result);boxes.extend(projection+bounds)
    x.mem_write(base+0x2000,bounds);x.mem_write(base+0x3000,struct.pack('<I4f',0,11,22,33,44));x.mem_write(stack,struct.pack('<5I',stop,base+0x1000+232,base+0x2000,base+0x200c,base+0x3000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(box_entry,stop,count=1000000)
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x3000,20))==result,case
probe=str(root/'build/pc/Release/rf_effect_probe.exe')
assert subprocess.check_output([probe,'--visibility-camera'],input=commands)==expected
assert subprocess.check_output([probe,'--box-project'],input=boxes)==box_expected
report=dict(result='PASS',cases=256,projected_boxes=visible,scope='Original 547150 setup followed by full 515d00 versus composed PC/NXDK camera and box projection. Exact unscaled/scaled bases, origin, frustum, projection environment and screen rectangles. Graphics-state call intercepted; native live scene integration excluded.')
(root/'artifacts/visibility-camera-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

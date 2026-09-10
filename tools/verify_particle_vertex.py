"""Original 551900 UV-only vertex writes, including color transform and fog."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
# Supply state binding and suppress index submission; execute all conversion
# instructions and actual fog/color helpers unchanged.
def hook(m,address,size,data):
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for address in (0x550850,0x559e80):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
entry=int(re.search(r'_rf_particle_vertex_encode\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(551900);commands=bytearray();results=bytearray()
for case in range(4096):
    rgba=rng.getrandbits(32);color=(0,1,256,257)[case%4];alpha=(0,1,256,257)[(case//4)%4];transform=(0,1,256,257)[(case//16)%4]
    depth_scale=(0,0.25,1,2)[case%4];rhw_scale=(0.5,1,2)[case%3];uv_scale=(0.5,2)
    fog_scale=(0,1,-1,0.125)[(case//64)%4];color_scale=tuple(rng.randint(-32,128)/64 for _ in range(3))
    environment=struct.pack('<4I8f',rgba,color,alpha,transform,depth_scale,rhw_scale,*uv_scale,fog_scale,*color_scale)
    camera=(0,0,(case%600)/2);screen=(rng.randint(-1000,1000)/4,rng.randint(-1000,1000)/4);inverse=(case%33-16)/8;uv=(rng.randint(-128,256)/64,rng.randint(-128,256)/64)
    vertex=struct.pack('<8f',*camera,*screen,inverse,*uv);commands.extend(environment+vertex)
    for address,value in ((0x17c7c14,rgba),(0x1cfcbd8,base+0x3000),(0x1e652f0,0),(0x1818348,0),(0x181834c,0),(0x1e652f4,0)):
        u.mem_write(address,struct.pack('<I',value))
    u.mem_write(0x5aa7e4,bytes([color&255,alpha&255]));u.mem_write(0x17c7c34,bytes([transform&255]));u.mem_write(0x17c7c38,struct.pack('<3f',*color_scale))
    u.mem_write(0x5a7dd8,struct.pack('<2f',depth_scale,rhw_scale));u.mem_write(0x1d86314,struct.pack('<f',uv_scale[0]));u.mem_write(0x1d4f2d4,struct.pack('<f',uv_scale[1]));u.mem_write(0x17c7c30,struct.pack('<f',fog_scale));u.mem_write(0x1e652ed,b'\1')
    u.mem_write(base+0x1000,vertex[:24]+bytes(4)+vertex[24:]+bytes(12));u.mem_write(base+0x1100,struct.pack('<I',base+0x1000));u.mem_write(base+0x3000,bytes([0xa5])*40)
    u.mem_write(stack,struct.pack('<5I',stop,1,base+0x1100,1,0x118c42));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x551900,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    assert bytes(u.mem_read(base+0x3020,8))==bytes([0xa5])*8
    result=bytes(4)+bytes(u.mem_read(base+0x3000,32));results.extend(result)
    x.mem_write(base,environment+vertex);x.mem_write(base+0x400,bytes([0xa5])*32)
    x.mem_write(stack,struct.pack('<4I',stop,base,base+48,base+0x400));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x400,32))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b])
actual=subprocess.check_output([str(c['probe']),'--particle-vertex-encode'],input=commands)
assert actual==results,[(i//36,i%36,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
report=dict(result='PASS',cases=4096,scope='Original 551900 UV-only draw flag 1, state binding and index submission supplied. Actual 550780, 52fc70, 52fcb0 and depth helper. First 32 GPU bytes exact PC/NXDK; untouched secondary UV tail verified. GPU rendering and live effects excluded.')
(root/'artifacts/particle-vertex-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

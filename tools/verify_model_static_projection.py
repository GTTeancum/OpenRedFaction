"""Compare static projection with the unchanged original vertex block."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_EDI,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;vertex=base+256;clip=0x1c0e700;world_pointer=base+1024
def stop(u,address,size,user):
    if address in (0x52e341,0x52e3e6):u.emu_stop()
u.hook_add(UC_HOOK_CODE,stop,begin=0x52e341,end=0x52e341)
u.hook_add(UC_HOOK_CODE,stop,begin=0x52e3e6,end=0x52e3e6)
rng=random.Random(52154);cases=[];expected=[];rejected=0
for n in range(4000):
    world=[rng.randint(-100,100)/16 for _ in range(3)];camera=[rng.randint(-100,100)/16 for _ in range(3)]
    rotation=[rng.randint(-16,16)/16 for _ in range(9)]
    if n%2==0:rotation=[1,0,0,0,1,0,0,0,1]
    if n>=2000:
        world=[rng.uniform(-100,100) for _ in range(3)];camera=[rng.uniform(-100,100) for _ in range(3)]
        rotation=[rng.uniform(-1,1) for _ in range(9)]
    fixed=rng.choice([0,1,2,-1]);factor=rng.randint(-100,100)/16
    screen=[320,-240,320,240];bounds=[0,0,640,480];far=rng.randrange(10)
    flags=[(n>>i)&1 for i in range(5)]
    if n%37==0:world[(n//37)%3]=float('nan')
    if n%41==0:world[(n//41)%3]=float('inf')
    case=struct.pack('<26f5I',*world,*camera,*rotation,fixed,factor,*screen,*bounds,far,*flags);cases.append(case)
    u.mem_write(esp,bytes(1024));u.mem_write(world_pointer,case[:12]);u.mem_write(vertex,b'\xa5'*40);u.mem_write(clip,b'\xa5'*12)
    for address,data in [(0x1818690,case[12:24]),(0x18186c8,case[24:60]),(0x1818b7c,struct.pack('<f',fixed)),(0x17c7c30,struct.pack('<f',factor)),(0x1cfcb3c,struct.pack('<4f',*bounds)),(0x1818b6c,struct.pack('<f',far))]:u.mem_write(address,data)
    for address,value in [(0x5a4d19,flags[0]),(0x5a4d18,flags[2]),(0x1818b65,flags[3])]:u.mem_write(address,bytes([value]))
    for offset,value in [(0x38,world_pointer),(0x34,0)]:u.mem_write(esp+offset,struct.pack('<I',value))
    for offset,value in [(0x78,screen[0]),(0x84,screen[1]),(0x80,screen[2]),(0x7c,screen[3])]:u.mem_write(esp+offset,struct.pack('<f',value))
    u.mem_write(esp+0x11,bytes(flags[1:2]));u.mem_write(esp+0x13,bytes(flags[4:5]))
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_ESI,vertex+18);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_EDI,0x1bdb2b4)
    u.emu_start(0x52e1b5,0,count=10000);end=u.reg_read(UC_X86_REG_EIP);assert end in (0x52e341,0x52e3e6)
    visible=end==0x52e341;rejected+=not visible
    cache=b'\xa5'*12+bytes(u.mem_read(0x1bdb2b0,12))+bytes(u.mem_read(0x1c3d554,1))+bytes(u.mem_read(0x1bf29b0,1))+b'\xa5'*6
    expected.append(cache+bytes(u.mem_read(clip,12))+bytes(u.mem_read(vertex,40))+struct.pack('<I',visible))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--project-static-vertex'],input=b''.join(cases))
assert len(actual)==len(cases)*88
for n,reference in enumerate(expected):
    observed=actual[n*88:n*88+88]
    assert observed==reference,(n,observed.hex(),reference.hex())
# Execute the compiled NXDK implementation, including its actual helpers.
p=pefile.PE(str(root/'build/xbox/main.exe'));binary=p.get_memory_mapped_image()
x=Uc(UC_ARCH_X86,UC_MODE_32);image_base=p.OPTIONAL_HEADER.ImageBase
x.mem_map(image_base,(len(binary)+4095)//4096*4096);x.mem_write(image_base,binary);x.mem_map(base,65536)
entry=int(re.search(r'\s_rf_model_project_static_vertex\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cache_address=base+4096;clip_address=base+4200;vertex_address=base+4300;visible_address=base+4400;end_address=base+60000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
args=[base,base+12,cache_address,clip_address,vertex_address,visible_address]
for n,case in enumerate(cases):
    x.mem_write(base,case);x.mem_write(cache_address,b'\xa5'*32);x.mem_write(clip_address,b'\xa5'*12);x.mem_write(vertex_address,b'\xa5'*40);x.mem_write(visible_address,pack(99))
    x.mem_write(esp,pack(end_address,*args));x.reg_write(UC_X86_REG_ESP,esp)
    x.emu_start(entry,end_address,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==end_address and x.reg_read(UC_X86_REG_EAX)==0,n
    observed=b''.join(bytes(x.mem_read(a,z)) for a,z in ((cache_address,32),(clip_address,12),(vertex_address,40),(visible_address,4)))
    assert observed==actual[n*88:n*88+88],('NXDK',n)
for null in range(6):
    invalid=args.copy();invalid[null]=0
    x.mem_write(cache_address,b'\xa5'*32);x.mem_write(clip_address,b'\xa5'*12);x.mem_write(vertex_address,b'\xa5'*40);x.mem_write(visible_address,pack(99))
    x.mem_write(esp,pack(end_address,*invalid));x.reg_write(UC_X86_REG_ESP,esp);x.emu_start(entry,end_address,count=10000)
    assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc
    assert bytes(x.mem_read(cache_address,32))==b'\xa5'*32 and bytes(x.mem_read(clip_address,12))==b'\xa5'*12 and bytes(x.mem_read(vertex_address,40))==b'\xa5'*40 and bytes(x.mem_read(visible_address,4))==pack(99)
report=dict(result='PASS',cases=len(cases),bit_exact_records=len(cases),nxdk_cases=len(cases),nxdk_null_guards=6,rejected=rejected,
    scope='Unchanged 0x52e1b5 to visibility branch, full 0x5475d0/0x52fc70 callees; all five gates, dyadic and arbitrary finite fixtures, signed/zero depth and non-finite input; all88 output bytes exact; supplied view globals, no draw claim')
(root/'artifacts/model-static-projection-verification.json').write_text(json.dumps(report,indent=2));print(report)

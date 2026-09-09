"""Compare fresh projection and clipping against unchanged original code."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;vertex=base+256;clip=base+512
def stop(u,address,size,user):
    if address in (0x52f31e,0x52f3cc):u.emu_stop()
u.hook_add(UC_HOOK_CODE,stop)
rng=random.Random(52154);cases=[];expected=[];rejected=0
for n in range(2000):
    world=[rng.randint(-100,100)/16 for _ in range(3)];camera=[rng.randint(-100,100)/16 for _ in range(3)]
    rotation=[rng.randint(-16,16)/16 for _ in range(9)]
    if n%2==0:rotation=[1,0,0,0,1,0,0,0,1]
    fixed=rng.choice([0,1,2,-1]);factor=rng.randint(-100,100)/16
    screen=[320,-240,320,240];bounds=[0,0,640,480];far=rng.randrange(10)
    flags=[(n>>i)&1 for i in range(5)]
    if n%37==0:world[(n//37)%3]=float('nan')
    if n%41==0:world[(n//41)%3]=float('inf')
    case=struct.pack('<26f5I',*world,*camera,*rotation,fixed,factor,*screen,*bounds,far,*flags);cases.append(case)
    u.mem_write(esp,bytes(1024));u.mem_write(0x1bdb2b0,case[:12]);u.mem_write(vertex,b'\xa5'*40);u.mem_write(clip,b'\xa5'*12)
    for address,data in [(0x1818690,case[12:24]),(0x18186c8,case[24:60]),(0x1818b7c,struct.pack('<f',fixed)),(0x17c7c30,struct.pack('<f',factor)),(0x1cfcb3c,struct.pack('<4f',*bounds)),(0x1818b6c,struct.pack('<f',far))]:u.mem_write(address,data)
    for address,value in [(0x5a4d19,flags[0]),(0x5a4d18,flags[2]),(0x1818b65,flags[3])]:u.mem_write(address,bytes([value]))
    for offset,value in [(0x14,vertex+18),(0x18,0),(0x64,clip)]:u.mem_write(esp+offset,struct.pack('<I',value))
    for offset,value in [(0x94,screen[0]),(0x90,screen[1]),(0x8c,screen[2]),(0x98,screen[3])]:u.mem_write(esp+offset,struct.pack('<f',value))
    u.mem_write(esp+0x12,bytes(flags[1:2]));u.mem_write(esp+0x13,bytes(flags[4:5]))
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_ESI,0);u.reg_write(UC_X86_REG_EBX,0x1bdb2b0)
    u.emu_start(0x52f154,0,count=10000);end=u.reg_read(UC_X86_REG_EIP);assert end in (0x52f31e,0x52f3cc)
    visible=end==0x52f31e;rejected+=not visible
    cache=b'\xa5'*12+bytes(u.mem_read(0x1bdb2b0,12))+bytes(u.mem_read(0x1c3d554,1))+bytes(u.mem_read(0x1bf29b0,1))+b'\xa5'*6
    expected.append(cache+bytes(u.mem_read(clip,12))+bytes(u.mem_read(vertex,40))+struct.pack('<I',visible))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--project-vertex'],input=b''.join(cases))
assert len(actual)==len(cases)*88
exact=0;max_error=0
for n,reference in enumerate(expected):
    observed=actual[n*88:n*88+88];exact+=observed==reference
    assert observed[:12]==reference[:12] and observed[24:32]==reference[24:32] and observed[44:]==reference[44:],(n,observed.hex(),reference.hex())
    for offset in (12,16,20,32,36,40):
        a=struct.unpack_from('<f',observed,offset)[0];b=struct.unpack_from('<f',reference,offset)[0]
        if math.isnan(b):assert math.isnan(a)
        elif math.isinf(b):assert a==b
        else:
            error=abs(a-b)/max(1,abs(b));max_error=max(max_error,error);assert error<=2e-6,(n,offset,a,b)
report=dict(result='PASS',cases=len(cases),bit_exact_records=exact,rejected=rejected,max_scaled_error=max_error,
    scope='Unchanged 0x52f154 to visibility branch, full 0x5475d0/0x52fc70 callees; all five gates, signed/zero depth and non-finite input; clip/depth/preserved bytes exact, projected floats tolerance 2e-6 * max(1,abs(reference)); supplied view globals, no draw claim')
(root/'artifacts/model-projection-verification.json').write_text(json.dumps(report,indent=2));print(report)

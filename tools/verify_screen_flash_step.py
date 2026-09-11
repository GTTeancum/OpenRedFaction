"""Original flash draw/decay orchestration; graphics terminals are recorded."""
import runpy
from pathlib import Path
globals().update(runpy.run_path(str(Path(__file__).with_name('verify_screen_flash.py'))))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_FPCW
entry=int(re.search(r'\s_rf_screen_flash_step\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
calls=[]
def graphics(m,address,size,user):
    if address not in (0x50cf80,0x50dbe0):return
    sp=m.reg_read(UC_X86_REG_ESP)
    n=4 if address==0x50cf80 else 5
    args=struct.unpack('<'+'I'*n,m.mem_read(sp+4,n*4));calls.append((address,args))
    m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0])
    m.reg_write(UC_X86_REG_ESP,sp+4)
original.hook_add(UC_HOOK_CODE,graphics)
commands=bytearray();expected=bytearray();rng=random.Random(0x4163c0)
for case in range(4096):
    alpha=(0,1,2,128,255,256,0x7fffffff,0x80000000,0xffffffff)[case%9]
    dt=(0.,.001,.005,.006,1/60,1/30,.5,2.)[(case//9)%8]
    freeze=(0,1,256,257,255)[(case//72)%5]
    state=rng.randbytes(4)+pack(alpha);wire=state+struct.pack('<fI',dt,freeze)
    owner=bytearray(rng.randbytes(0x1200));owner[0x10d0:0x10d8]=state
    original.mem_write(base,bytes(owner));original.mem_write(0x5a4014,struct.pack('<f',dt))
    original.mem_write(0x637086,bytes([freeze&255]));original.mem_write(0x17c7bf4,pack(640,480))
    original.mem_write(0x17756c0,pack(0x12345678));calls.clear()
    original.mem_write(stack,pack(stop,base));original.reg_write(UC_X86_REG_ESP,stack)
    original.reg_write(UC_X86_REG_FPCW,0x27f);original.emu_start(0x4163c0,stop,count=10000)
    assert original.reg_read(UC_X86_REG_EIP)==stop
    current=bytes(original.mem_read(base+0x10d0,8));owner[0x10d0:0x10d8]=current
    assert bytes(original.mem_read(base,len(owner)))==owner
    active=int(bool(calls));draw=bytes([0xa5])*8
    if active:
        assert len(calls)==2 and calls[0][0]==0x50cf80 and calls[1]==(0x50dbe0,(0,0,640,480,0x12345678)),calls
        color=calls[0][1];draw=bytes(v&255 for v in color)+pack(color[3])
    result=current+draw+pack(active)
    xbox.mem_write(base,state+bytes([0xa5])*12)
    xbox.mem_write(stack,pack(stop,base)+wire[8:]+pack(base+8,base+16))
    xbox.reg_write(UC_X86_REG_ESP,stack);xbox.reg_write(UC_X86_REG_FPCW,0x27f)
    xbox.emu_start(entry,stop,count=10000)
    assert xbox.reg_read(UC_X86_REG_EIP)==stop and xbox.reg_read(UC_X86_REG_EAX)==0
    assert bytes(xbox.mem_read(base,20))==result,case
    commands.extend(wire);expected.extend(result)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--screen-flash-step'],input=commands)
assert actual==expected,'PC differs'
report=dict(result='PASS',cases=4096,original_sha256=digest,
 scope='Original4163c0 including real viewport getters, freeze getter and ftol; color and rectangle graphics terminals recorded. PC/NXDK exact pre-decay color, draw gate, low-byte freeze and truncated170*dt decay. Full owner preserved outside alpha. Nonnegative finite dt only; no GPU pixels or campaign integration.')
(root/'artifacts/screen-flash-step.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))

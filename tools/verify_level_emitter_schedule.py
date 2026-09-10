"""Execute both original frame emitter loops with actual room eligibility checks."""
import runpy,struct,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,base,stack,root=(c[k] for k in ('u','base','stack','root'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
u.mem_map(base+0x10000,0x20000)
def put(a,v):u.mem_write(a,struct.pack('<I',v))
calls=[]
def update(m,a,size,data):
    if a!=0x4972f0:return
    calls.append(m.reg_read(UC_X86_REG_ECX))
    sp=m.reg_read(UC_X86_REG_ESP)
    ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,update)
total=0
for count in (0,1,2,17,87,128):
    put(0x646080,count);put(0x646088,base+0x1000)
    for phase,(start,end) in enumerate(((0x433374,0x4333bb),(0x433455,0x433493))):
        expected=[];calls.clear()
        for i in range(count):
            emitter=base+0x10000+i*344;room=base+0x20000+i*384
            put(base+0x1000+i*4,emitter)
            present=(i+phase)%5!=0;visible=(0,1,255,256)[(i+phase)%4]
            put(emitter+0x4c,room if present else 0);put(room+0x160,visible)
            # Enable is deliberately opposite; caller gating must ignore it.
            put(emitter+0x140,0 if visible&255 else 1)
            if present and visible&255:expected.append(emitter)
        u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(start,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end and calls==expected,(count,phase)
        assert u.reg_read(UC_X86_REG_ESP)==stack
        total+=len(calls)
report=dict(result='PASS',passes=12,update_calls=total,collection='0x646080',
    scope='Actual original 433374..4333bb and 433455..433493 loops, vector accessors and 497390 room-byte check. Update callee intercepted to record emitter pointers. Same collection, ordered traversal, null rooms, low-byte visibility, changed visibility between passes, independent emitter enable. Full frame/simulation/rendering excluded.')
(root/'artifacts/level-emitter-schedule-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

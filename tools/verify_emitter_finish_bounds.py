"""Original 497df0 visibility radius finalization against PC/NXDK."""
import runpy, struct, re, random, subprocess, json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_emitter_pool_finish_bounds\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def put(machine,address,value):machine.mem_write(address,struct.pack('<I',value&0xffffffff))
commands=bytearray();expected=bytearray();rng=random.Random(0x497df0)
updated=0
for case in range(2048):
    enabled=(0,1,255,256,257,0xffffffff)[case%6]
    owner=(-1,-2,0,1)[case//6%4]
    maximum=(0,-0.0,rng.randint(0,1000000)/128)[case%3]
    radius=rng.randint(-1000,1000)/128;previous=-17.25
    command=struct.pack('<Ii3f',enabled,owner,maximum,radius,previous);commands.extend(command)
    u.mem_write(base,bytes(344));put(u,base+4,owner)
    u.mem_write(base+0x48,struct.pack('<f',radius));u.mem_write(base+0x9c,struct.pack('<2f',previous,maximum))
    put(u,base+0x148,0x7bd6e8);put(u,0x7bd830,base);put(u,0x59fd1c,enabled)
    put(u,stack,stop);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x497df0,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(4)+bytes(u.mem_read(base+0xa0,4))+bytes(u.mem_read(base+0x9c,4));expected.extend(result)
    updated+=bool(enabled&255) and owner>=0
    pool=base+0x2000;slot=base+0x3000
    x.mem_write(pool,struct.pack('<7I',slot,0,128,128,0,0,1));x.mem_write(slot,bytes(228))
    put(x,slot+184,owner);x.mem_write(slot+200,struct.pack('<2f',maximum,previous))
    x.mem_write(slot+68,struct.pack('<f',radius));put(x,slot+216,129)
    x.mem_write(stack,struct.pack('<3I',stop,pool,enabled));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(slot+200,8))
    assert actual==result,(case,actual.hex(),result.hex())
assert subprocess.check_output([str(c['probe']),'--emitter-finish-bounds'],input=commands)==expected
report=dict(result='PASS',cases=2048,updated=updated,scope='Original 497df0 and actual 4973e0 accessor versus PC/NXDK, single active slot; global low byte, negative-owner bypass, exact radius and accumulator reset. Full-list/campaign/native XEMU integration excluded.')
(root/'artifacts/emitter-finish-bounds-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)


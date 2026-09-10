"""Full original attached emission versus PC/NXDK; real lookup/class predicate."""
import runpy,struct,re,json,subprocess,random
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_emission_trace.py')))
u=c['u'];x=c['c']['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop'];put=c['put'];get=c['get'];fput=c['fput']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
entry=int(re.search(r'_rf_particle_emitter_emit_parent\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000)
storage=base+0x10000;lists=base+0x8000;pool=base+0x8100
emitter=c['emitter'];node=c['node'];parent=base+0x4000;free=c['free'];thread=c['thread']
rng=random.Random(496);commands=bytearray();expected=bytearray()
for case in range(2048):
    fixture=c['fixtures'][case];empty=fixture['empty'];seed=fixture['seed'];now=fixture['now_ms']
    owner=0x10001 if case%7 else -1;present=case%5!=0
    before=bytearray.fromhex(fixture['before']);struct.pack_into('<i',before,4,owner)
    flags=struct.unpack_from('<I',before,0x38)[0]|(0x40 if case&8 else 0)|(0x80 if case&16 else 0)
    struct.pack_into('<I',before,0x38,flags)
    struct.pack_into('<3f',before,0x14,*(rng.randint(1,10000)/1024 for _ in range(3)))
    pos=tuple(rng.randint(-10000,10000)/256 for _ in range(3))
    basis=((1,0,0,0,1,0,0,0,1),(0,0,-1,0,1,0,1,0,0),(2,0.25,0,0,1,0.125,0,0,0.5))[case%3]
    velocity=tuple(rng.randint(-1000,1000)/256 for _ in range(3))
    life=(-1,0,0.1,10)[case%4];class_index=(-1,0,1,2)[(case//4)%4];class_flags=0x40 if case&32 else 0
    effective_flags=class_flags if class_index==0 else 0
    parent_bytes=struct.pack('<15fIfI',*pos,*basis,*velocity,0x10001,life,effective_flags)
    u.mem_write(emitter,bytes(before));u.mem_write(parent,bytes(0x2a8));u.mem_write(node,bytes([0xa5])*120)
    put(0x7394cc+4,parent if present else 0);put(parent+0x2c,0x10001)
    fput(parent+0x34,life);fput(parent+0x3c,*pos);fput(parent+0x48,*basis);fput(parent+0x144,*velocity)
    put(parent+0x2a4,class_index);put(0x872448,1);put(0x85cf70,class_flags)
    for sentinel in (free,emitter+0xb0):put(sentinel,sentinel);put(sentinel+4,sentinel)
    if not empty:put(free,node);put(free+4,node);put(node,free);put(node+4,free)
    put(0x7a3cf4,0);put(thread+20,seed);put(0x5a3ed8,now)
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,emitter)
    u.emu_start(0x496c50,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    after=bytes(u.mem_read(emitter,0x158));compact=bytes(before[4:0x9c])+bytes(before[0x154:0x158])
    result_emitter=after[4:0x9c]+after[0x154:0x158]
    commands.extend(compact+struct.pack('<3I',seed,now,empty)+parent_bytes+struct.pack('<I',present))
    particle=bytearray(u.mem_read(node,120));particle[:8]=struct.pack('<2I',501,1601) if empty else struct.pack('<2I',1605,1605)
    if not empty:particle[0x68:0x6c]=struct.pack('<I',1)
    result=struct.pack('<iII',-3 if empty else 0,get(thread+20),0xa5a5a5a5 if empty else 500)+result_emitter+particle
    expected.extend(result)
    x.mem_write(base,compact);x.mem_write(base+0x200,struct.pack('<II',seed,0xa5a5a5a5));x.mem_write(base+0x300,parent_bytes)
    x.mem_write(pool,struct.pack('<5I',storage,lists,6,0,0))
    for i in range(6):x.mem_write(lists+i*8,struct.pack('<2I',1600+i,1600+i))
    if not empty:x.mem_write(lists+8,struct.pack('<2I',500,1599))
    x.mem_write(storage+500*120,struct.pack('<2I',501,1601)+bytes([0xa5])*112)
    x.mem_write(storage+501*120,struct.pack('<2I',502,500))
    x.mem_write(stack,struct.pack('<8I',stop,pool,base,1,now,base+0x300 if present else 0,base+0x200,base+0x204))
    x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x200,8))+bytes(x.mem_read(base,156))+bytes(x.mem_read(storage+500*120,120))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
actual=subprocess.check_output([str(c['c']['probe']),'--particle-emission-parent'],input=commands)
assert len(actual)==len(expected),(len(actual),len(expected))
assert actual==expected,[(i//288,i%288,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report=dict(result='PASS',cases=2048,scope='Full original 496c50 with unchanged 496bc0, real 40a0e0 lookup and 4c90f0 class predicate; exact emitter/particle/RNG/timer versus PC/NXDK. Translation, rotation, nonunit basis, inheritance flags, missing/ignored parents, class index bounds and exhaustion. Caller supplies resolved parent/class view; campaign object lifecycle and rendering excluded.')
(root/'artifacts/particle-emission-parent-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

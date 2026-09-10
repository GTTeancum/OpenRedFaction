"""Full original emitter update with actual phase, timer, emission and parent room."""
import runpy,struct,re,json,subprocess,random
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_emission_trace.py')))
u=c['u'];x=c['c']['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop'];put=c['put'];get=c['get'];fput=c['fput']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
entry=int(re.search(r'_rf_particle_emitter_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000)
storage=base+0x10000;lists=base+0x8000;pool=base+0x8100
emitter=c['emitter'];node=c['node'];parent=base+0x4000;free=c['free'];thread=c['thread']
from unicorn import UC_HOOK_CODE
observed=[]
def watch(m,a,size,data):
    if a in (0x496f60,0x4fa3f0,0x496c50,0x40a490):observed.append(a)
u.hook_add(UC_HOOK_CODE,watch)
def compact_state(raw):
    return bytes(raw[4:0x9c])+bytes(raw[0x154:0x158])+bytes(raw[0x128:0x138])+bytes(raw[0x140:0x144])+bytes(raw[0x13c:0x140])+bytes(raw[0x138:0x13c])
rng=random.Random(4972);commands=bytearray();expected=bytearray();emissions=created_total=toggles=0
for case in range(2048):
    fixture=c['fixtures'][case];empty=fixture['empty'];seed=fixture['seed'];now=fixture['now_ms']
    owner=0x10001 if case%7 else -1;present=case%5!=0
    before=bytearray.fromhex(fixture['before']);struct.pack_into('<i',before,4,owner)
    flags=struct.unpack_from('<I',before,0x38)[0]|(0x40 if case&8 else 0)|(0x80 if case&16 else 0)
    flags|=(4 if case&64 else 0)|(32 if case&128 else 0)
    struct.pack_into('<I',before,0x38,flags)
    global_enabled=(0,1,256,257,2)[case%5];dt=(0,0.016,0.25,2)[case%4]
    enabled=(0,1,2,256,257,0xa5a50001)[case%6]
    deadline=(-1,now,now+25,now-25)[case%4]
    struct.pack_into('<i',before,0x154,deadline)
    struct.pack_into('<4f',before,0x128,0.25,0.125,0.5,0.25)
    struct.pack_into('<ffI',before,0x138,0.5,(case%7)/8,enabled)
    room=0x43210000+case
    put(0x59fd1c,global_enabled&255);fput(0x5a4014,dt)
    struct.pack_into('<3f',before,0x14,*(rng.randint(1,10000)/1024 for _ in range(3)))
    pos=tuple(rng.randint(-10000,10000)/256 for _ in range(3))
    basis=((1,0,0,0,1,0,0,0,1),(0,0,-1,0,1,0,1,0,0),(2,0.25,0,0,1,0.125,0,0,0.5))[case%3]
    velocity=tuple(rng.randint(-1000,1000)/256 for _ in range(3))
    life=(-1,0,0.1,10)[case%4];class_index=(-1,0,1,2)[(case//4)%4];class_flags=0x40 if case&32 else 0
    effective_flags=class_flags if class_index==0 else 0
    parent_bytes=struct.pack('<15fIfI',*pos,*basis,*velocity,0x10001,life,effective_flags)
    u.mem_write(emitter,bytes(before));u.mem_write(parent,bytes(0x2a8));u.mem_write(node,bytes([0xa5])*120)
    put(0x7394cc+4,parent if present else 0);put(parent+0x2c,0x10001);put(parent,room)
    fput(parent+0x34,life);fput(parent+0x3c,*pos);fput(parent+0x48,*basis);fput(parent+0x144,*velocity)
    put(parent+0x2a4,class_index);put(0x872448,1);put(0x85cf70,class_flags)
    for sentinel in (free,emitter+0xb0):put(sentinel,sentinel);put(sentinel+4,sentinel)
    if not empty:put(free,node);put(free+4,node);put(node,free);put(node+4,free)
    put(0x7a3cf4,0);put(thread+20,seed);put(0x5a3ed8,now)
    observed.clear()
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,emitter)
    u.emu_start(0x4972f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    after=bytes(u.mem_read(emitter,0x158));compact=compact_state(before)
    result_runtime=compact_state(after)
    emitted=0x496c50 in observed;created=emitted and not empty
    emissions+=emitted;created_total+=created;toggles+=0x496f60 in observed
    actions=struct.pack('<5I',0x496f60 in observed,0x4fa3f0 in observed,emitted,created,500 if created else 0xffffffff)
    commands.extend(compact+struct.pack('<3I',seed,now,empty)+parent_bytes+struct.pack('<IIfI',present,global_enabled,dt,room))
    particle=bytearray(u.mem_read(node,120));particle[:8]=struct.pack('<2I',1605,1605) if created else struct.pack('<2I',501,1601)
    if created:particle[0x68:0x6c]=struct.pack('<I',1)
    result=struct.pack('<II',0,get(thread+20))+result_runtime+actions+particle
    expected.extend(result)
    x.mem_write(base,compact);x.mem_write(base+0x200,struct.pack('<I',seed));x.mem_write(base+0x220,bytes([0xa5])*20);x.mem_write(base+0x300,parent_bytes)
    x.mem_write(pool,struct.pack('<5I',storage,lists,6,0,0))
    for i in range(6):x.mem_write(lists+i*8,struct.pack('<2I',1600+i,1600+i))
    if not empty:x.mem_write(lists+8,struct.pack('<2I',500,1599))
    x.mem_write(storage+500*120,struct.pack('<2I',501,1601)+bytes([0xa5])*112)
    x.mem_write(storage+501*120,struct.pack('<2I',502,500))
    x.mem_write(stack,struct.pack('<5If5I',stop,pool,base,1,global_enabled,dt,now,base+0x300 if present else 0,room,base+0x200,base+0x220))
    x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x200,4))+bytes(x.mem_read(base,184))+bytes(x.mem_read(base+0x220,20))+bytes(x.mem_read(storage+500*120,120))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
actual=subprocess.check_output([str(c['c']['probe']),'--particle-update'],input=commands)
assert len(actual)==len(expected),(len(actual),len(expected))
assert actual==expected,[(i//332,i%332,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report=dict(result='PASS',cases=2048,emissions=emissions,created=created_total,toggles=toggles,scope='Full unchanged original 4972f0 phase/timer/emission/parent-room update versus PC/NXDK. Exact runtime state, particle payload, actions and RNG, with original lookup/class helpers and actual allocation. Campaign frame hookup, existing-particle simulation and rendering excluded.')
(root/'artifacts/particle-update-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

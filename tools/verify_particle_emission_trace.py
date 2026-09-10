"""Execute full original unattached emitter emission, allocation and timer reset.

Produces ignored reference fixtures for the pending shared emitter integration.
No particle, random, vector or timer routines are replaced.
"""
import runpy, struct, random, json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,root,base,stack,stop,thread=(c[k] for k in ('u','root','base','stack','stop','thread'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
def put(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def fput(a,*v):u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
emitter=base+0x2000;node=base+0x3000;free=0x7a3c04
trace=[]
def observe(m,a,size,data):
    if a in (0x4fae00,0x504e40,0x496840,0x57312d,0x4fa360):
        sp=m.reg_read(UC_X86_REG_ESP)
        trace.append((a,get(thread+20),bytes(m.mem_read(sp+4,28)).hex()))
u.hook_add(UC_HOOK_CODE,observe)
rng=random.Random(0x496c50);fixtures=[]
for case in range(512):
    empty=bool(case&1);orientation=bool(case&2);dependent=bool(case&4)
    cosine=(-2,-1,0,0.9999,1,2)[(case//8)%6]
    direction=((0,0,0),(0,1,0),(1,2,3),(0,-2,0))[(case//48)%4]
    seed=rng.getrandbits(32);now=(0,1072799900,10000)[case%3]
    u.mem_write(emitter,bytes(0x158));u.mem_write(node,bytes([0xa5])*120)
    put(emitter+4,-1);fput(emitter+8,1,2,3);fput(emitter+0x14,*direction)
    fput(emitter+0x20,cosine,2,8,0.5,0.1,0.3)
    put(emitter+0x38,8 if dependent else 0);fput(emitter+0x3c,1,3,0.2,0.7)
    put(emitter+0x4c,0x12345678)
    packet=emitter+0x50
    fput(packet+0x1c,0.1,0.2,1);put(packet+0x2c,23);put(packet+0x30,16)
    put(packet+0x34,0x44332211);put(packet+0x38,0x88776655)
    put(packet+0x3c,0x200 if orientation else 0);fput(packet+0x44,0.5)
    for sentinel in (free,emitter+0xb0):put(sentinel,sentinel);put(sentinel+4,sentinel)
    if not empty:put(free,node);put(free+4,node);put(node,free);put(node+4,free)
    put(0x7a3cf4,0);put(thread+20,seed);put(0x5a3ed8,now)
    before=bytes(u.mem_read(emitter,0x158));trace.clear()
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,emitter)
    u.emu_start(0x496c50,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    order=[a for a,_,_ in trace]
    expected_order=([0x4fae00,0x504e40,0x57312d,0x57312d] if cosine<1 else [])
    expected_order += [0x504e40,0x57312d]*3+[0x496840]
    expected_order += [0x504e40,0x57312d] if orientation and not empty else []
    expected_order += [0x504e40,0x57312d,0x4fa360]
    assert order==expected_order,(case,order)
    next_seed=seed
    for _ in range(order.count(0x57312d)):next_seed=(next_seed*214013+2531011)&0xffffffff
    assert get(thread+20)==next_seed
    # Interval remains extended precision until milliseconds are truncated.
    delay=int(((f32(0.3)-f32(0.1))*((next_seed>>16)&32767)/32768+f32(0.1))*1000)
    deadline=now+delay
    if deadline>1072800000:deadline-=1072800000
    assert get(emitter+0x154)==deadline,(case,get(emitter+0x154),deadline)
    allocation=next(t for t in trace if t[0]==0x496840)
    assert struct.unpack('<7I',bytes.fromhex(allocation[2]))==(1,packet,0x12345678,emitter+8,0xffffffff,0,emitter)
    assert get(0x7a3cf4)==(0 if empty else 1)
    assert get(emitter+0xb0)==(emitter+0xb0 if empty else node)
    if empty:assert bytes(u.mem_read(node,120))==bytes([0xa5])*120
    else:
        assert bytes(u.mem_read(node+0xc,24))==bytes(u.mem_read(packet,24))
        assert get(node+8)==0xffffffff and get(node+0x68)==emitter
    assert bytes(u.mem_read(emitter+0x20,4))==struct.pack('<f',max(-1,min(1,cosine)) if cosine<1 else cosine)
    fixtures.append(dict(case=case,empty=empty,seed=seed,now_ms=now,before=before.hex(),after=bytes(u.mem_read(emitter,0x158)).hex(),particle=bytes(u.mem_read(node,120)).hex(),rng=next_seed,trace=trace))
out=root/'artifacts/particle-emission-trace.json'
report=dict(result='PASS',cases=len(fixtures),created=256,exhausted=256,scope='Full unchanged original 496c50, parentless 496bc0, cone/vector/math/RNG helpers, 496840 allocation and 4fa360 timer. Proves branch and draw order, allocation arguments, pool exhaustion and timer wrapping. Shared C emitter integration and parent-owned emission remain open.')
out.write_text(json.dumps(dict(report=report,fixtures=fixtures),indent=2)+'\n');print(report)

"""Full original 497020 initialization with a supplied room, no traversal query.

Retains reference fixtures for shared emitter creation; original emission,
allocation, normalization, phase duration, RNG and timer execute unchanged.
"""
import runpy,struct,json,random
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,root,base,stack,stop,thread=(c[k] for k in ('u','root','base','stack','stop','thread'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
def put(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
emitter=base+0x2000;source=base+0x3000;node=base+0x4000;free=0x7a3c04
calls=[]
def observe(m,a,size,data):
    if a in (0x496c50,0x496f60,0x57312d,0x4fa360):calls.append((a,get(emitter+0x140),get(thread+20)))
u.hook_add(UC_HOOK_CODE,observe)
rng=random.Random(0x497020);fixtures=[];immediate=created=phase_count=0
for case in range(1024):
    flags=(0x10 if case&1 else 0)|(2 if case&2 else 0)|(0x20 if case&4 else 0)|(8 if case&8 else 0)
    enabled=(0,1,2,256,257)[case%5];empty=bool(case&16);seed=rng.getrandbits(32);now=(0,10000,1072799900)[case%3]
    raw=bytearray(132)
    struct.pack_into('<I6f',raw,0,0x12345678,1,2,3,*(rng.randint(1,10000)/1024 for _ in range(3)))
    struct.pack_into('<6fI',raw,28,0.5 if case&32 else 2,2,8,0.1,0.3,0.5,0xbeef0000|flags)
    struct.pack_into('<11f',raw,56,1,3,0.2,0.7,0.1,0.2,1,0.25,0.125,0.5,0.25)
    struct.pack_into('<6IfI',raw,100,23,16,0x44332211,0x88776655,0x200 if case&64 else 0,0,0.5,0x11223344)
    before=bytearray([0xa5])*0x158
    struct.pack_into('<I',before,0x154,0xffffffff)
    u.mem_write(emitter,bytes(before));u.mem_write(source,bytes(raw));u.mem_write(node,bytes([0xa5])*120)
    put(free,free);put(free+4,free)
    if not empty:put(free,node);put(free+4,node);put(node,free);put(node+4,free)
    put(0x7a3cf4,0);put(thread+20,seed);put(0x5a3ed8,now);calls.clear()
    u.mem_write(stack,struct.pack('<6I',stop,0xffffffff,source,0x2468,0,enabled))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,emitter)
    u.emu_start(0x497020,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    after=bytes(u.mem_read(emitter,0x158));emit=bool(flags&0x10 and flags&2);allocated=emit and not empty;alternate=bool(flags&0x20)
    immediate+=emit;created+=allocated;phase_count+=alternate
    assert sum(a==0x496c50 for a,_,_ in calls)==emit
    assert sum(a==0x496f60 for a,_,_ in calls)==alternate
    assert sum(a==0x4fa360 for a,_,_ in calls)==bool(flags&0x10)
    if emit:assert next(v for a,v,_ in calls if a==0x496c50)==0xa5a5a5a5
    if alternate:
        assert next(v for a,v,_ in calls if a==0x496f60)==(0xa5a5a500|(enabled&255))
        assert after[0x13c:0x140]==bytes(4)
    else:assert after[0x138:0x140]==before[0x138:0x140]
    draws=(2 if emit and case&32 else 0)+(4 if emit else (1 if flags&0x10 else 0))+(1 if allocated and case&64 else 0)+alternate
    next_seed=seed
    for _ in range(draws):next_seed=(next_seed*214013+2531011)&0xffffffff
    assert get(thread+20)==next_seed and sum(a==0x57312d for a,_,_ in calls)==draws
    assert get(emitter+0x38)==0xa5a50000|flags
    assert get(emitter+0x140)==0xa5a5a500|(enabled&255)
    assert get(emitter+0x4c)==0x2468 and get(0x7a3cf4)==allocated
    if not flags&0x10:assert after[0x154:0x158]==before[0x154:0x158]
    assert after[0x98:0xa0]==before[0x98:0xa0] # Opaque spawn callback and adjacent field untouched.
    assert after[0x144:0x148]==raw[128:132]
    assert get(emitter+0xb0)==(node if allocated else emitter+0xb0)
    if not emit:
        for a,b in ((0x5c,0x6c),(0x78,0x7c)):assert after[a:b]==before[a:b]
    fixtures.append(dict(case=case,flags=flags,enabled=enabled,empty=empty,seed=seed,now=now,source=raw.hex(),before=before.hex(),after=after.hex(),particle=bytes(u.mem_read(node,120)).hex(),rng=next_seed,calls=calls))
report=dict(result='PASS',cases=1024,immediate_attempts=immediate,created=created,phase_initializations=phase_count,scope='Full original 497020 supplied-room/no-traversal path, parentless owner; unchanged normalization, immediate emission/allocation, timer and phase initialization. Verifies partial-width stores, retained fields and RNG ordering. Shared C initializer, room traversal and campaign lifecycle remain open.')
(root/'artifacts/particle-emitter-init-trace.json').write_text(json.dumps(dict(report=report,fixtures=fixtures),indent=2)+'\n');print(report)

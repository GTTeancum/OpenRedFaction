"""Original fixed emitter pool allocation, exhaustion, release and FIFO reuse."""
import runpy,struct,json,random
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_emitter_init_trace.py')))
u,root,base,stack,stop,thread=(c[k] for k in ('u','root','base','stack','stop','thread'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
def put(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def call(address,*args,end=None):
    u.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(address,stop if end is None else end,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==(stop if end is None else end)
    return u.reg_read(UC_X86_REG_EAX)
first=0x7b2a70;active=0x7bd6e8;free=0x7bd840;detached=0x7bd670
u.mem_write(first,bytes(128*344));u.mem_map(0,4096);call(0x496a70)
call(0x4973ff,end=0x49745a) # Only fixed list setup, without table parsing/atexit registration.
put(detached,detached);put(detached+4,detached);put(0x7bd998,0)
call(0x494e70)
source=base+0x8000;raw=bytearray.fromhex(c['fixtures'][3]['source'])
struct.pack_into('<I',raw,52,0x12);u.mem_write(source,bytes(raw));put(thread+20,123)
free_nodes=[first+i*344 for i in range(128)];active_nodes=[];detached_nodes=[];events=[]
def chain(sentinel,nodes,offset):
    all_nodes=[sentinel]+nodes
    for i,node in enumerate(all_nodes):
        assert get(node+offset)==all_nodes[(i+1)%len(all_nodes)]
        assert get(node+offset+4)==all_nodes[i-1]
def check():
    chain(free,free_nodes,0x148);chain(active,active_nodes,0x148);chain(detached,detached_nodes,0)
    assert get(0x7bd998)==len(active_nodes)
    assert get(0x7a3cf4)==len(active_nodes)+len(detached_nodes)
def allocate(now):
    variation=random.Random(now+497)
    for offset in (32,36,76,80):struct.pack_into('<f',raw,offset,variation.randint(-5000,5000)/512)
    for offset in (56,60,64,68):struct.pack_into('<f',raw,offset,variation.randint(1,5000)/512)
    u.mem_write(source,bytes(raw))
    put(0x5a3ed8,now);seed=get(thread+20)
    before=bytes(u.mem_read(free_nodes[0],344)) if free_nodes else None
    node=call(0x497ca0,0xffffffff,source,0x2468,0,0)
    if not free_nodes:
        assert node==0 and get(thread+20)==seed;events.append(dict(operation='exhausted'));check();return
    assert node==free_nodes.pop(0);active_nodes.append(node)
    particle=get(node+0xb0);assert particle!=node+0xb0 and get(particle+0x68)==node
    assert get(node+0xa0)==0
    # No blanket reset on reuse: fields outside initialization/bounds/link work persist.
    after=bytes(u.mem_read(node,344))
    for a,b in ((0x98,0x9c),(0xa4,0xb0),(0x138,0x140)):
        assert after[a:b]==before[a:b]
    events.append(dict(operation='allocate',source=bytes(raw).hex(),now=now,slot=(node-first)//344,seed=seed,rng=get(thread+20),before=before.hex(),after=after.hex(),particle=bytes(u.mem_read(particle,120)).hex()))
    check()
def release(index):
    node=active_nodes.pop(index)
    put(node+0x98,0x11223344);u.mem_write(node+0xa4,struct.pack('<3f',1.25,-2.5,7.0));u.mem_write(node+0x138,struct.pack('<2f',0.75,0.125))
    before=bytes(u.mem_read(node,344));particle=get(node+0xb0)
    particle_before=bytearray(u.mem_read(particle,120));call(0x497d80,node);free_nodes.append(node);detached_nodes.append(particle)
    after=bytes(u.mem_read(node,344));expected=bytearray(before)
    expected[0xb0:0xb8]=struct.pack('<II',node+0xb0,node+0xb0);expected[0x148:0x150]=after[0x148:0x150]
    assert after==expected
    particle_before[:8]=u.mem_read(particle,8);particle_before[0x68:0x6c]=bytes(4)
    assert bytes(u.mem_read(particle,120))==particle_before
    events.append(dict(operation='release',slot=(node-first)//344,after=after.hex(),particle=bytes(u.mem_read(particle,120)).hex()));check()
check()
for i in range(128):allocate(i*10)
allocate(2000)
for i in range(64):release((i*13)%len(active_nodes))
for i in range(64):allocate(3000+i*10)
allocate(4000)
while active_nodes:release(len(active_nodes)//2)
for i in range(128):allocate(5000+i*10)
report=dict(result='PASS',capacity=128,allocations=320,releases=192,exhaustions=2,live_emitters=len(active_nodes),detached_particles=len(detached_nodes),scope='Original 497ca0/497d80, full initializer/emission and particle detachment unchanged; fixed-list setup replayed after static construction, table parsing omitted. Every list link/count checked after each operation; release preserves payload and live particles, FIFO slot reuse retains unassigned state. Shared emitter pool and campaign integration remain open.')
(root/'artifacts/emitter-pool-trace.json').write_text(json.dumps(dict(report=report,events=events),indent=2)+'\n');print(report)

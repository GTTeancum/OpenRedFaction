"""Original lazy portal projection/traversal/cache reset versus PC and NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_visibility_view_scale.py')))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
for machine in (u,x):machine.mem_map(base+0x10000,0x20000)
symbols=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
def invoke(machine,address,args):
    machine.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args))
    machine.reg_write(UC_X86_REG_ESP,stack);machine.emu_start(address,stop,count=2000000)
    assert machine.reg_read(UC_X86_REG_EIP)==stop
    return machine.reg_read(UC_X86_REG_EAX)
def put(address,value):u.mem_write(address,struct.pack('<I',value))
calls={0x507ba0:0,0x518750:0,0x515d00:0}
def count_calls(machine,address,size,data):
    if address in calls:calls[address]+=1
u.hook_add(UC_HOOK_CODE,count_calls)
ends=((0,1),(1,2),(2,3),(3,0),(0,2));links=(0,3,4,0,1,1,2,4,2,3);ranges=((0,3),(3,2),(5,3),(8,2))
rng=random.Random(0x4d4c20);commands=bytearray();expected=bytearray();changed=0;unvisited=0
for case in range(128):
    width,height=(640,480) if case%2 else (1280,720);mode=int(case%7!=0)
    origin=[rng.randint(-4,4)*0.25 for _ in range(3)]
    basis=(1,0,0,0,1,0,0,0,1) if case%3 else (.8,0,.6,0,1,0,-.6,0,.8)
    fov=90.;far=100.;far_enabled=case%2
    camera=struct.pack('<4i3fI13f3If',width,height,-7,13,1.,fov,far,mode,*origin,*basis,.1,1,far_enabled,1,0.)
    start=case%4;special=(case//4)%5;flags=int(case%19==0);rect=(0.,0.,float(width),float(height))
    rooms=[];portals=[];caches=[]
    for i,(first,count) in enumerate(ranges):rooms.extend((first,count,int(case%11==i),int(case%7==i)))
    for i,(a,b) in enumerate(ends):
        kind=(case+i)%5
        lo=((-1.,-1.,-1.),(-2.,-2.,3.),(100.,100.,3.),(-1.,-1.,-8.),(-4.,-3.,4.))[kind]
        hi=tuple(v+(2. if kind!=4 else 8.) for v in lo)
        portal=base+0x20000+i*64;u.mem_write(portal,bytes(64));put(portal,base+0x10000+a*384);put(portal+4,base+0x10000+b*384)
        u.mem_write(portal+8,struct.pack('<6f',*lo,*hi));u.mem_write(portal+0x24,bytes((1,1)))
        u.mem_write(portal+0x28,struct.pack('<4f',11,22,33,44))
        portals.append(struct.pack('<3I4f',a,b,1,11,22,33,44));caches.append(struct.pack('<6fI',*lo,*hi,1))
    packet=camera+struct.pack('<3I4f16I10I',start,special,flags,*rect,*rooms,*links)+b''.join(portals+caches)
    assert len(packet)==512;commands.extend(packet);x.mem_write(base+0x9000,packet)
    world=base+0x3000;put(world+0xb4,5);put(world+0xbc,base+0x3500)
    for i in range(5):put(base+0x3500+i*4,base+0x20000+i*64)
    for i,p in enumerate(links):put(base+0x4000+i*4,base+0x20000+p*64)
    outputs=[]
    for stage in range(3):
        if stage==1:
            origin[0]+=8;x.mem_write(base+0x9020,struct.pack('<f',origin[0]))
        u.mem_write(0x17c7bec,struct.pack('<4i',-7,13,width,height));u.mem_write(0x17c7bd8,struct.pack('<f',1.))
        put(0x17c7bc4,1920);put(0x17c7bc8,1080) # Full render dimensions deliberately differ from viewport.
        u.mem_write(0x1818b68,struct.pack('<f',far));u.mem_write(0x1818b74,struct.pack('<f',.1))
        u.mem_write(0x1818b65,bytes([far_enabled]));u.mem_write(0x5a4d18,b'\1');u.mem_write(0x5a445a,b'\1')
        u.mem_write(0x1e652e8,bytes(4));put(0x17c7bcc,0x66)
        u.mem_write(base,struct.pack('<9f',*basis));u.mem_write(base+64,struct.pack('<3f',*origin))
        invoke(u,0x547150,(base,base+64,struct.unpack('<I',struct.pack('<f',fov))[0],0,mode))
        for i,(first,count) in enumerate(ranges):
            address=base+0x10000+i*384;u.mem_write(address,bytes(384));u.mem_write(address,bytes([rooms[i*4+2]]))
            put(address+0x40,rooms[i*4+3]);put(address+0x164,255);put(address+0x30,count);put(address+0x38,base+0x4000+first*4)
        put(0x9bb588,base+0x10000+special*384 if special<4 else 0);put(0x9bb57c,0);u.mem_write(0x9a8548,bytes(16))
        if stage!=1:
            before=[bytes(u.mem_read(base+0x20000+i*64,56)) for i in range(5)]
            invoke(u,0x4d4c20,(world,))
            for i,raw in enumerate(before):
                clean=bytearray(raw);clean[0x24]=0
                assert bytes(u.mem_read(base+0x20000+i*64,56))==clean
        previous_calls=dict(calls)
        u.mem_write(base+0x5000,struct.pack('<4f',*rect));invoke(u,0x4d4860,(world,base+0x10000+start*384,base+0x5000,0,flags,base+64))
        if stage==1:assert previous_calls==calls # Entire walk reuses the preceding cache.
        out=bytearray(4)+u.mem_read(0x9bb57c,4)
        for i in range(4):
            raw=bytes(u.mem_read(base+0x10000+i*384,384))
            out.extend(struct.pack('<2I',raw[0x160],raw[0x161])+raw[0x164:0x168]+raw[0x16c:0x17c])
        for p in struct.unpack('<4I',u.mem_read(0x9a8548,16)):out.extend(struct.pack('<I',(p-base-0x10000)//384 if p else 0))
        validity=[]
        for i,(a,b) in enumerate(ends):
            raw=bytes(u.mem_read(base+0x20000+i*64,56));out.extend(struct.pack('<3I',a,b,raw[0x25])+raw[0x28:0x38]);validity.append(raw[0x24])
        out.extend(struct.pack('<5I',*validity));expected.extend(out);outputs.append(out);unvisited+=validity.count(0)
        state=base+0x6000;storage=base+0x7000;order=base+0x8000;packet_address=base+0x9000;scratch=base+0x10000
        x.mem_write(base+0x2000,bytes(328));assert invoke(x,symbol('rf_visibility_camera_setup'),(packet_address,base+0x2000))==0
        x.mem_write(storage,bytes(112));x.mem_write(order,bytes(16));x.mem_write(state,struct.pack('<4I',storage,order,4,0))
        assert invoke(x,symbol('rf_visibility_begin_view'),(state,))==0
        if stage!=1:assert invoke(x,symbol('rf_visibility_portals_begin_view'),(packet_address+372,5))==0
        x.mem_write(base+0x2500,struct.pack('<6I',base+0x2000,packet_address+372,packet_address+232,5,1920,1080))
        status=invoke(x,symbol('rf_visibility_traverse_projected'),(state,packet_address+128,packet_address+192,10,base+0x2500,start,special,flags,packet_address+112,scratch))
        actual=struct.pack('<I',status)+bytes(x.mem_read(state+12,4))+bytes(x.mem_read(storage,112))+bytes(x.mem_read(order,16))+bytes(x.mem_read(packet_address+232,140))
        actual+=b''.join(x.mem_read(packet_address+372+i*28+24,4) for i in range(5))
        assert actual==out,(case,stage,[(i,a,b) for i,(a,b) in enumerate(zip(actual,out)) if a!=b][:16],
            [struct.unpack_from('<3I4f',v,136+i*28) for v in (out,actual) for i in range(5)])
    assert outputs[0]==outputs[1]
    changed+=outputs[1]!=outputs[2]
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--visibility-projected'],input=commands)==expected
assert changed>0 and unvisited>0 and all(calls.values())
report=dict(result='PASS',cases=128,walks=384,camera_reset_changed_cases=changed,untouched_portal_observations=unvisited,
    original_calls={hex(k):v for k,v in calls.items()},scope='Full original 547150/4d4860/4d4c20 with actual classification, box clipping, projection and recursion; only camera graphics-state call intercepted. Exact PC/NXDK room state, order, cache validity, rejection and rectangles. Changed-camera reuse without reset and recomputation after reset. Synthetic cyclic graphs; authored room metadata and native live renderer integration remain excluded.')
(root/'artifacts/visibility-projected-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

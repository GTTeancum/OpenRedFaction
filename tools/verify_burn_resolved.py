"""Original attachment/model/movement/emission chain vs concrete PC/NXDK backend."""
import hashlib,json,re,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ev=runpy.run_path(str(root/'tools/verify_loaded_room_locator.py'))
u,x=ev['u'],ev['x'];w=ev['w'];stack,stop=ev['stack'],ev['stop']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
b=0x40000000
for m in (u,x):m.mem_map(b,1024*1024)
record=b;owner=b+0x2000;emitters=[b+0x3000+i*0x200 for i in range(4)]
pose=b+0x10000;desc=b+0x14000;handle=b+0x15000;thread=b+0x16000
storage=b+0x20000;slots=b+0x60000;pool=b+0x70000;particles=pool+64;lists=pool+128
context=b+0x71000;output=context+128;random=context+160;parent=context+192
symbols=(root/'build/xbox/main.map').read_text()
def call(name,*args):
    entry=int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
    x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=10000000)
    assert x.reg_read(UC_X86_REG_EIP)==stop,('NXDK limit',name)
    return x.reg_read(UC_X86_REG_EAX)
def put(address,*values):u.mem_write(address,w(*values))
def fp(address,*values):u.mem_write(address,struct.pack('<'+'f'*len(values),*values))
def tls(m,address,size,data):
    if address!=0x577eef:return
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    m.reg_write(UC_X86_REG_EAX,thread);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,tls)
def room_token(pointer):return ev['roomptrs'].index(pointer)+1 if pointer else 0
def compact(raw):
    result=bytearray(raw[4:0x9c]+raw[0x154:0x158]+raw[0x128:0x138]+raw[0x140:0x144]+raw[0x13c:0x140]+raw[0x138:0x13c])
    struct.pack_into('<I',result,72,room_token(struct.unpack_from('<I',raw,0x4c)[0]));return bytes(result)
ticks=list(range(0,4001,500));poses=subprocess.check_output([str(root/'build/pc/Release/rf_skeleton_probe.exe'),str(root/'Installed_Game/meshes.vpp'),str(root/'Installed_Game/motions.vpp'),'miner.v3c','ult2_stand.rfa'],input=w(*ticks))
stride=len(poses)//len(ticks);bones=(11,12,15,8)
commands=[];expected=[];emitted=0
nodes=[0x782748+i*120 for i in range(4)];free=0x7a3c04
links={free:1601,**{p:500+i for i,p in enumerate(nodes)},**{p+0xb0:1605+i for i,p in enumerate(emitters)}}
for case in range(72):
    # The first sample is the sampler's unevaluated cache. Use advanced poses.
    frame=1+case%(len(ticks)-1);matrix=b''.join(poses[frame*stride+i*48:frame*stride+(i+1)*48] for i in bones)
    elapsed=(0.,12.,12.000001,17.)[case%4];enabled=(0,1,257)[(case//4)%3]
    owner_id=0xffffffff if case&16 else 0x10001;flags=(0,0x40,0x20)[(case//3)%3]
    now=1000;dt=.25;room=ev['primary'][case%ev['primary_count']];rp=ev['roomptrs'][room]
    parent_data=struct.pack('<15fIfI',1,2,3,1,0,0,0,1,0,0,0,1,.1,.2,.3,0x10001,10,0)
    u.mem_write(owner,bytes(0x300));put(owner,rp);put(owner+0x2c,0x10001);fp(owner+0x34,10)
    u.mem_write(owner+0x3c,parent_data[:48]);u.mem_write(owner+0x144,parent_data[48:60]);put(owner+0x2a4,0xffffffff)
    put(0x7394d0,owner);put(0x6460e8,ev['solid']);put(owner+0x80,handle)
    u.mem_write(pose,bytes(0x3000));u.mem_write(pose,matrix);put(pose+0x1d50,desc);put(desc+0x48,4);put(handle,2,pose)
    u.mem_write(record,bytes(64));put(record,*emitters,0x10001,0,1,2,3);fp(record+0x30,elapsed)
    initial=[]
    for i,p in enumerate(emitters):
        u.mem_write(p,bytes(0x158));put(p+4,owner_id);fp(p+8,0,0,0);fp(p+0x14,0,1,0)
        fp(p+0x20,1,2,4,.1,.1,.2);put(p+0x38,flags);fp(p+0x3c,1,2,.1,.2);put(p+0x4c,rp)
        fp(p+0x50+0x1c,.1,.2,1);put(p+0x50+0x2c,23,1,0xffffffff,0xff000000)
        fp(p+0x128,.25,0,.5,0);fp(p+0x138,.25,0);put(p+0x140,1);put(p+0x154,900 if case&1 else 1100)
        put(p+0xb0,p+0xb0,p+0xb0);initial.append(compact(bytes(u.mem_read(p,0x158))))
    for i,p in enumerate(nodes):u.mem_write(p,w(nodes[i+1] if i<3 else free,nodes[i-1] if i else free)+bytes(112))
    put(free,nodes[0],nodes[-1]);put(0x7a3cf4,0);put(thread+20,123);put(0x5a3ed8,now);put(0x59fd1c,enabled&255);fp(0x5a4014,dt)
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,record);u.reg_write(UC_X86_REG_EBX,owner);u.reg_write(UC_X86_REG_FPCW,0x27f)
    end=0x42f0bd if elapsed<=12 else 0x42f1dc
    u.emu_start(0x42ef3e,end,count=10000000);assert u.reg_read(UC_X86_REG_EIP)==end,('original limit',case)
    particle_bytes=[]
    for p in nodes:
        raw=bytearray(u.mem_read(p,120));a,c=struct.unpack_from('<2I',raw);struct.pack_into('<2I',raw,0,links[a],links[c])
        if struct.unpack_from('<I',raw,0x58)[0]&1:
            emitted+=1;struct.pack_into('<I',raw,0x64,room_token(struct.unpack_from('<I',raw,0x64)[0]))
            struct.pack_into('<I',raw,0x68,emitters.index(struct.unpack_from('<I',raw,0x68)[0])+1)
        particle_bytes.append(bytes(raw))
    want=w(0)+matrix[2*48+36:3*48]+w(int(elapsed<=12))+bytes(u.mem_read(thread+20,4))+b''.join(compact(bytes(u.mem_read(p,0x158))) for p in emitters)+b''.join(particle_bytes)
    commands.append(b''.join(initial)+matrix+parent_data+w(owner_id,now,room+1,enabled)+struct.pack('<2f',dt,elapsed));expected.append(want)
    assert call('rf_particle_pool_init',particles,storage,lists,133)==0
    assert call('rf_emitter_pool_init',pool,slots,particles)==0
    x.mem_write(lists+8,w(500,503))
    for i in range(4):
        x.mem_write(storage+(500+i)*120,w(501+i if i<3 else 1601,499+i if i else 1601))
        x.mem_write(slots+i*228,initial[i]);x.mem_write(slots+i*228+216,w(i+1 if i<3 else 129,i-1 if i else 129,1))
    x.mem_write(pool+8,w(4,127,0,3,4));x.mem_write(slots+4*228+220,w(128))
    x.mem_write(record,w(1,2,3,4,0x10001,0,1,2,3)+bytes(12)+struct.pack('<f',elapsed)+bytes(12))
    x.mem_write(pose,matrix);x.mem_write(parent,parent_data);x.mem_write(random,w(123));x.mem_write(output,bytes(16))
    x.mem_write(context,w(pose,4,pool,ev['world'],0 if owner_id==0xffffffff else parent,random,owner_id,now,room+1,enabled)+struct.pack('<f',dt))
    status=call('rf_burn_attachments_resolved',record,context,output)
    got=w(status)+bytes(x.mem_read(output,16))+bytes(x.mem_read(random,4))+b''.join(bytes(x.mem_read(slots+i*228,184)) for i in range(4))+bytes(x.mem_read(storage+500*120,480))
    assert got==want,('NXDK',case,[(i,a,c) for i,(a,c) in enumerate(zip(got,want)) if a!=c][:20])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--burn-resolved',str(root/'Installed_Game'/ev['archive']),ev['level']],input=b''.join(commands))
assert actual==b''.join(expected),('PC',len(actual),len(b''.join(expected)),[(i,a,c) for i,(a,c) in enumerate(zip(actual,b''.join(expected))) if a!=c][:20])
assert emitted>0,'Fixture must exercise real emission, not just placement'
saved_record=bytes(x.mem_read(record,64));saved_slots=bytes(x.mem_read(slots,4*228))
for guard in range(3):
    x.mem_write(record,saved_record);x.mem_write(slots,saved_slots)
    if guard==0:x.mem_write(record,w(0))
    elif guard==1:x.mem_write(record+4,w(1))
    else:x.mem_write(slots,w(1234567))
    before=[bytes(x.mem_read(a,n)) for a,n in ((record,64),(slots,4*228),(output,16),(random,4))]
    assert call('rf_burn_attachments_resolved',record,context,output)==0xfffffffc
    assert before==[bytes(x.mem_read(a,n)) for a,n in ((record,64),(slots,4*228),(output,16),(random,4))]
report=dict(result='PASS',cases=len(commands),particles=emitted,nxdk_guards=3,level=ev['level'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original attachment region with real cached model query, real world movement, full phase/emission/parent-room update and four-node allocation vs PC/NXDK concrete backend. Only CRT thread-storage lookup supplied. Sampled miner standing bones; owner state supplied; no animation advancement, entity creation, particle simulation/rendering or native XEMU execution.')
(root/('artifacts/burn-resolved-'+ev['level']+'.json')).write_text(json.dumps(report,indent=2)+'\n');print(report)

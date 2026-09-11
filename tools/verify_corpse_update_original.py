"""Complete original417290 with real timers/vectors and supplied model/sound effects."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;emit=b+0x1000;sound=b+0x3000;duration_ptr=b+0x4000;stub=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda v:struct.pack('<f',v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(b,65536)
u.reg_write(UC_X86_REG_FPCW,0x27f);u.mem_write(stub,b'\xdd\x05'+w(duration_ptr)+b'\xc3')
def read(a,n):return list(struct.unpack('<'+'I'*n,u.mem_read(a,4*n)))
counts={0x5033f0:1,0x5033b0:4,0x5033e0:2,0x503360:6,0x4164c0:1,0x459a20:1,0x48ac70:2,0x48a230:1}
trace=[];found=0;reset_mutation=0;initial_model=0
point=struct.pack('<3f',1.25,-2,3)
def hook(m,a,size,data):
    if a not in counts:return
    sp=m.reg_read(UC_X86_REG_ESP);ret=read(sp,1)[0];args=read(sp+4,counts[a]);result=0;pop=0
    if a==0x48ac70:
        assert args[0]==b;m.mem_write(args[1],point);args=[args[0]]
    elif a==0x48a230:
        assert m.reg_read(UC_X86_REG_ECX)==sound and bytes(m.mem_read(args[0],12))==point
        args=[sound,*struct.unpack('<3I',point)];pop=4
    elif a==0x459a20:result=sound if found else 0
    elif a==0x4164c0:assert not read(b+0x29c,1)[0]&8
    trace.append([a,args])
    if a==0x5033f0 and reset_mutation:
        m.mem_write(b+0x80,w(initial_model+100));m.mem_write(b+0x2b8,w(7))
    if a==0x5033e0:m.reg_write(UC_X86_REG_EIP,stub);return
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x417290);digest=hashlib.sha256();transitions=shutdowns=followed=mutations=0
scale=struct.unpack('<f',p.get_data(0x589520-p.OPTIONAL_HEADER.ImageBase,4))[0]
for case in range(4096):
    health=rng.choice((-1.,0.,100.));fade=rng.choice((-.1,0.,.1,1.));dt=rng.choice((0.,.25,.5,1.))
    flags=rng.getrandbits(32);objflags=rng.getrandbits(32);deadline=rng.choice((-1,0,999,1000,1001));class_value=rng.choice((0.,1.,100.))
    value=rng.choice((-1.,0.,.25,10.));initial_model=rng.choice((0,123));motion=rng.choice((-1,0,2));sound_id=rng.choice((-1,9));found=rng.randrange(2)
    reset_mutation=rng.randrange(2);duration=rng.choice((0.,.3333333333333333,1.5,17.0000001));n=rng.randrange(5)
    body=bytearray(rng.randbytes(0x400));sound_body=bytearray(rng.randbytes(0x200));emitter_bodies=[]
    for off,v in ((0x34,health),(0x298,fade),(0x2b0,value)):body[off:off+4]=f(v)
    for off,v in ((0x7c,objflags),(0x80,initial_model),(0x29c,flags),(0x2a0,0),(0x2ac,deadline),(0x2b8,motion),(0x2cc,sound_id),(0x268,emit if n else 0)):body[off:off+4]=w(v)
    for i in range(n):
        e=bytearray(rng.randbytes(0x180));e[0x150:0x154]=w(emit+(i+1)*0x200 if i+1<n else 0);emitter_bodies.append(e);u.mem_write(emit+i*0x200,bytes(e))
    input_body=bytes(body);input_emitters=[bytes(e) for e in emitter_bodies]
    u.mem_write(b,bytes(body));u.mem_write(sound,bytes(sound_body));u.mem_write(duration_ptr,struct.pack('<d',duration))
    u.mem_write(0x5a4014,f(dt));u.mem_write(0x5a3ed8,w(1000));u.mem_write(0x5cd8e4,f(class_value));u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack);trace=[]
    u.emu_start(0x417290,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=[]
    def call(a,*args):expected.append([a,[v&0xffffffff for v in args]])
    if health<0:body[0x7c:0x80]=w(objflags|2)
    else:
        if flags&1:
            remaining=struct.unpack('<f',f(fade))[0]-dt;body[0x298:0x29c]=f(remaining)
            if remaining<=0:body[0x7c:0x80]=w(objflags|2)
        if 0<=deadline<=1000:
            body[0x2ac:0x2b0]=w(-1);shutdowns+=1
            for e in emitter_bodies:e[0x140]=0
        if value>0:body[0x2b0:0x2b4]=f(value-((dt*scale)*class_value))
        model=initial_model
        if motion>=0 and flags&8:
            transitions+=1;call(0x5033f0,model)
            if reset_mutation:
                mutations+=1;model+=100;motion=7;body[0x80:0x84]=w(model);body[0x2b8:0x2bc]=w(motion)
            call(0x5033b0,model,motion,0x3f800000,1);call(0x5033e0,model,motion)
            call(0x503360,model,struct.unpack('<I',f(duration))[0],0,0,0,1)
            call(0x503360,model,0x3e99999a,0,0,0,1)
            flags&=~8;body[0x29c:0x2a0]=w(flags);call(0x4164c0,b)
        if sound_id!=-1:
            call(0x459a20,sound_id)
            if found:
                followed+=1;call(0x48ac70,b);sound_body[0xf0:0xfc]=point;call(0x48a230,sound,*struct.unpack('<3I',point))
        if model:call(0x503360,model,struct.unpack('<I',f(dt))[0],0,b+0x3c,b+0x48,1)
    assert trace==expected,(case,trace,expected)
    assert bytes(u.mem_read(b,len(body)))==body,case
    assert bytes(u.mem_read(sound,len(sound_body)))==sound_body,case
    for i,e in enumerate(emitter_bodies):assert bytes(u.mem_read(emit+i*0x200,len(e)))==e,case
    digest.update(json.dumps(trace).encode())
    if 'observe_case' in globals():observe_case(globals())
report=dict(result='PASS',cases=4096,animation_transitions=transitions,reset_mutations=mutations,emitter_shutdowns=shutdowns,sound_follows=followed,trace_sha256=digest.hexdigest(),original_sha256=sha,scope='Full417290 with real timer expiry/invalidation, deletion/fading helpers and vector initialization/copy. Supplied model, pose, sound lookup/position effects; duration returns x87 double. Exact actor/emitter/sound writes and ordered callback arguments, including model/motion mutation during reset. No shared implementation comparison or live resources.')
(root/'artifacts/corpse-update-original.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

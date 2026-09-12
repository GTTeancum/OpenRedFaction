"""Execute full original4104a0 with explicit resource/allocation boundaries.

Synthetic class/owner views verify orchestration, not the supplied services.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x200000);u.mem_map(0,4096)
OBJ,NAME,POS,MATRIX,STACK,STOP=B+0x1000,B+0x3000,B+0x4000,B+0x4100,B+0xe000,B+0xf000
CLS=0x5afb88;SENTINEL=0x5c9360
w=lambda *a:struct.pack('<%dI'%len(a),*(v&0xffffffff for v in a))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def cstr(a):
    result=bytearray()
    while u.mem_read(a,1)!=b'\0':result+=u.mem_read(a,1);a+=1
    return bytes(result)
def string(a,value):
    global pool
    u.mem_write(pool,value+b'\0');u.mem_write(a,w(len(value),pool));pool+=len(value)+1

BOUNDARIES=(0x486da0,0x4ffa80,0x4ffa20,0x4ff470,0x434da0,0x5056a0,
 0x497ca0,0x5006c0,0x4ffd60,0x503220,0x413d20,0x413f20,
 0x412470,0x4c1ec0,0x48c9a0,0x50ea00)
def hook(cpu,address,size,context):
    global emitted
    if address not in BOUNDARIES:return
    sp=rsp=cpu.reg_read(UC_X86_REG_ESP);owner=cpu.reg_read(UC_X86_REG_ECX)
    arg=lambda i:r(sp+4+4*i)
    result=pop=0
    if address==0x486da0:
        assert [arg(i) for i in (0,1,2,4,5)]==[4,0xffffffff,123,0x100000 if shield else 0,0]
        descriptor=bytes(cpu.mem_read(arg(3),152));expected=bytearray(152)
        expected[:8]=w(r(CLS+12),kind);expected[16:20]=w(material);expected[60:72]=position
        expected[72:108]=matrix;expected[132:136]=radius
        expected[148:152]=w(0x20 if flags&6 else 0)
        assert descriptor==expected,('descriptor',case)
        trace.append(('allocate',));result=0 if fail else OBJ
    elif address in (0x4ffa80,0x4ffa20):
        value=cstr(arg(0)) if address==0x4ffa80 else cstr(r(arg(0)+4))
        string(owner,value);result=owner;pop=4
    elif address==0x4ff470:pass
    elif address==0x434da0:
        assert [arg(i) for i in range(5)]==[sound,OBJ+0x3c,0x3f800000,0x173c378,0]
        trace.append(('sound',sound));result=0x7654
    elif address==0x5056a0:
        assert arg(0)==0x7654;trace.append(('sound-handle',));result=0x4567
    elif address==0x497ca0:
        assert arg(0)==handle and arg(2)==r(OBJ) and arg(3)==OBJ+0x3c and arg(4)==1
        index=(arg(1)-0x70000000)//256;trace.append(('emitter',index))
        if index%2==0:
            result=B+0x8000+emitted*0x200;cpu.mem_write(result,b'\xa5'*0x160);emitted+=1
    elif address==0x5006c0:
        string(arg(0),str(arg(1)).encode());result=arg(0)
    elif address==0x4ffd60:
        string(arg(0),cstr(arg(1))+cstr(r(arg(2)+4)));result=arg(0)
    elif address==0x503220:
        assert arg(0)==model;name=cstr(arg(1));trace.append(('tag',name.decode()))
        if name.startswith(b'corona_') and name[7:].isdigit():
            number=int(name[7:]);result=100+number if number<=coronas else -1
        elif name==b'corona_rod1':result=51 if rod else -1
        elif name==b'corona_rod2':result=52
        elif name==b'light_prop':result=77
        else:raise AssertionError(name)
    elif address==0x413d20:
        assert arg(0)==handle and arg(2)==(glare&0xffffffff) and arg(3)==0
        trace.append(('glare',arg(1)))
    elif address==0x413f20:
        assert [arg(i) for i in range(5)]==[handle,rodclass,51,52,0xffffffff]
        trace.append(('rod',))
    elif address==0x412470:
        assert [arg(i) for i in range(5)]==[handle,0xffffffff,64,32,1];trace.append(('screen',))
    elif address==0x4c1ec0:
        assert arg(0)==explosion;trace.append(('explosion',))
    elif address==0x48c9a0:
        assert arg(0)==OBJ;trace.append(('collision',))
        assert r(0x5c9358)==initial_count and r(SENTINEL+0x290)==SENTINEL
    elif address==0x50ea00:
        assert owner==0x5c97e8 and arg(0)==slot and arg(1)==1
        assert r(0x5c9358)==initial_count+1 and r(SENTINEL+0x290)==OBJ
        trace.append(('slot',slot));pop=8
    cpu.reg_write(UC_X86_REG_EAX,result&0xffffffff);cpu.reg_write(UC_X86_REG_EIP,r(rsp))
    cpu.reg_write(UC_X86_REG_ESP,rsp+4+pop)
u.hook_add(UC_HOOK_CODE,hook)
if '--shared' in sys.argv:
    from verify_clutter_factory_shared import verify as verify_shared
else:verify_shared=None
rng=random.Random(0x4104a0);totals=dict(success=0,failure=0,emitters=0,glare=0,slots=0)
for case in range(512):
    pool=B+0x10000;trace=[];emitted=0;fail=case%11==0;shield=bool(case&1)
    kind=rng.choice((1,3));material=rng.randrange(9);flags=rng.randrange(0x1000);initial_class_flags=flags
    life=rng.choice((-1.,0.,10.,100.));lifetime=rng.choice((-1.,0.,.5,1.25))
    sound=rng.choice((-1,0,12));explosion=rng.choice((-1,3));glare=rng.choice((-1,2))
    rod=bool(case&2);rodclass=rng.choice((-1,4));coronas=rng.randrange(8)
    emitters=[rng.choice((-1,0,1,2)) for _ in range(rng.randrange(5))]
    position=struct.pack('<3f',*(rng.uniform(-100,100) for _ in range(3)))
    matrix=struct.pack('<9f',*(rng.uniform(-1,1) for _ in range(9)))
    radius=struct.pack('<f',rng.choice((-1.,1.,3.)));handle=55;model=0x3456
    slot=rng.choice((0,1,3199,3200));register=rng.choice((0,1,2));initial_count=7
    before=bytearray(rng.randbytes(0x2d8));before[:4]=w(0x12345678)
    before[0x2c:0x30]=w(handle);before[0x80:0x84]=w(model);before[0x268:0x26c]=w(0)
    objectflags=rng.getrandbits(24);before[0x7c:0x80]=w(objectflags)
    u.mem_write(OBJ,bytes(before));u.mem_write(CLS,b'\0'*232)
    string(CLS,b'fixture');string(CLS+8,b'fixture.v3d');string(CLS+16,b'fixture' if case&4 else b'')
    u.mem_write(CLS+0x28,w(len(emitters),len(emitters),B+0x5000));u.mem_write(B+0x5000,w(*emitters))
    for i in range(3):u.mem_write(0x7b2770+i*4,w(0x70000000+i*256))
    u.mem_write(CLS+0x34,struct.pack('<fIf',lifetime,kind,life));u.mem_write(CLS+0x40,radius)
    u.mem_write(CLS+0x4c,w(material));u.mem_write(CLS+0x50,w(sound));u.mem_write(CLS+0x58,w(explosion))
    u.mem_write(CLS+0x74,w(flags,0,glare,rodclass))
    cached=[201,202] if flags&0x400 else [];u.mem_write(CLS+0x84,w(len(cached),*cached))
    u.mem_write(CLS+0x98,w(99));u.mem_write(CLS+0xe0,w(64,32))
    class_before=bytearray(u.mem_read(CLS,232))
    u.mem_write(NAME,b'\0' if case&8 else b'instance\0');u.mem_write(POS,position);u.mem_write(MATRIX,matrix)
    u.mem_write(0x5c97dc,w(1));u.mem_write(0x5afb78,w(0 if shield else -1))
    u.mem_write(0x5afb84,w(slot));u.mem_write(0x5a3ed8,w(12345))
    u.mem_write(SENTINEL+0x28c,w(SENTINEL,SENTINEL));u.mem_write(0x5c9358,w(initial_count))
    u.mem_write(STACK,w(STOP,0,NAME,123,POS,MATRIX,register));u.reg_write(UC_X86_REG_ESP,STACK)
    try:u.emu_start(0x4104a0,STOP,count=1000000)
    except Exception:
        print('factory stopped',case,hex(u.reg_read(UC_X86_REG_EIP)),trace);raise
    assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==STACK+4
    assert u.reg_read(UC_X86_REG_EAX)==(0 if fail else OBJ)
    if fail:
        assert trace==[('allocate',)] and bytes(u.mem_read(OBJ,len(before)))==before
        assert bytes(u.mem_read(CLS,232))==class_before and r(0x5c9358)==initial_count
        if verify_shared:verify_shared(locals())
        totals['failure']+=1;continue
    expected_trace=[('allocate',)];expected=before[:]
    expected[0x18:0x20]=u.mem_read(OBJ+0x18,8)
    assert cstr(r(OBJ+0x1c))==(b'fixture' if case&8 else b'instance')
    expected[0x294:0x29c]=w(CLS,0);expected[0x2cc]=0
    finalflags=objectflags|(0x100000 if flags&0x20 else 0)|(0x40000 if flags&2 else 0)|(0x1000 if flags&1 else 0)|(4 if life<0 else 0)
    expected[0x7c:0x80]=w(finalflags);expected[0x34:0x3c]=struct.pack('<fI',100. if life<0 else life,0)
    expected[0x29c:0x2ac]=w(0 if case&4 else -1,0x4567 if sound>=0 else -1,-1,-1)
    expected[0x2b0:0x2c0]=w(12345+int(lifetime*1000+.5) if lifetime>0 else -1,-1,0,-1)
    expected[0x2d0:0x2d4]=w(-1)
    if sound>=0:expected_trace.extend([('sound',sound),('sound-handle',)])
    accepted=[i for i in emitters if i>=0]
    expected_trace.extend(('emitter',i) for i in accepted)
    live=[i for i in accepted if i%2==0]
    expected[0x268:0x26c]=w(B+0x8000+(len(live)-1)*0x200 if live else 0)
    for i in range(len(live)):assert r(B+0x8000+i*0x200+0x150)==(B+0x8000+(i-1)*0x200 if i else 0)
    if glare!=-1 and not flags&0x400:
        expected_trace.extend(('tag','corona_'+str(i)) for i in range(1,coronas+2))
        cached=list(range(101,101+min(coronas,4)));class_before[0x84:0x88+4*len(cached)]=w(len(cached),*cached);flags|=0x400
    expected_trace.extend(('glare',i) for i in cached)
    expected_trace.append(('tag','corona_rod1'))
    if rod:
        expected_trace.append(('tag','corona_rod2'))
        if rodclass>=0:expected_trace.append(('rod',))
    if flags&0x10 and not flags&0x800:
        expected_trace.append(('tag','light_prop'));class_before[0x98:0x9c]=w(77);flags|=0x800
    if flags&8:expected_trace.append(('screen',))
    if explosion!=-1:expected_trace.append(('explosion',))
    if before[0x1a8]&0x20 and not finalflags&0x8000:expected_trace.append(('collision',))
    expected[0x28c:0x294]=w(SENTINEL,SENTINEL)
    allocated=bool(register and slot<3200);expected[0x2d4:0x2d6]=struct.pack('<H',slot if allocated else 65535)
    if allocated:expected_trace.append(('slot',slot))
    class_before[0x70:0x78]=w(12345,flags)
    assert trace==expected_trace,(case,'trace',trace,expected_trace)
    assert bytes(u.mem_read(OBJ,len(expected)))==expected,(case,'object footprint')
    assert bytes(u.mem_read(CLS,232))==class_before,(case,'class footprint')
    assert r(0x5afb84)==slot+allocated and r(0x5c9358)==initial_count+1
    assert r(SENTINEL+0x28c)==OBJ and r(SENTINEL+0x290)==OBJ
    if verify_shared:verify_shared(locals())
    totals['success']+=1;totals['emitters']+=len(accepted);totals['glare']+=len(cached);totals['slots']+=allocated
result=dict(result='PASS',cases=512,**totals,original_sha256=digest,
 scope='Full4104a0 with supplied generic allocation, string storage, model tags, sound/emitter/glare/screen/explosion/collision/slot effects. Actual descriptor constructors, vector/matrix copies, class lookup/comparison, array reads and timers execute. Synthetic finite class inputs; complete object/class write footprint and ordered calls. No shared C factory, resource implementation, malformed rod assertion, invalid class boundary, or native XEMU claim.')
result['compiled_nxdk']=bool(verify_shared)
if verify_shared:
    from verify_clutter_factory_shared import guards
    result['nxdk_error_cases']=guards();result['pc']=True
    result['scope']='Full original4104a0 versus shared PC/compiled NXDK factory over synthetic finite class/owner inputs, normalized field footprints, ordered resource requests and four-entry corona cache. Actual original fixed array append executes. Resource implementations and native XEMU/live integration remain unproved.'
(ROOT/'artifacts/clutter-factory.json').write_text(json.dumps(result,indent=2));print(result)

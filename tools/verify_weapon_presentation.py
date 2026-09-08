"""Compare original presentation control flow and all mutations to operation boundaries."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
player,strings,stack=[0x30000000+i*0x100000 for i in range(3)]
for a in (player,strings,stack):u.mem_map(a,65536)
u.mem_write(strings,b'model\0');u.mem_write(strings+32,b'\0')
boundaries={0x4a73b0:'clear',0x50ce00:'resource',0x4b0610:'mode',0x50ba90:'missing_model'}
for a in boundaries:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=a,end=a)
rng=random.Random(0x4ae0d0);cases=[]
for k in range(4000):
    weapon=rng.choice([-2,-1,0,1,2,3,31,63,64]);current=rng.choice([-1,weapon,0,1,3])
    state=struct.pack('<IIiii',rng.choice([0,0,0x1111]),rng.getrandbits(32),current,7,100)
    context=struct.pack('<I4iIi',rng.choice([0,1,1,1]),0,1,2,3,rng.choice([0,1,255,256]),rng.choice([weapon,-1]))
    descriptors=b''.join(struct.pack('<IIi',rng.choice([0,1,1,1]),rng.choice([0,0x2000+i]),rng.choice([-1,-1,-1,5])) for i in range(64))
    cache=b''.join(struct.pack('<iII',rng.choice([-99,-99,i]),rng.choice([0,0x3000+i]),rng.choice([0,0x4000+i])) for i in range(32))
    cases.append(state+struct.pack('<i',weapon)+context+descriptors+cache)
out=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--presentation'],input=b''.join(cases));assert len(out)==28*len(cases)
coverage={**{v:0 for v in boundaries.values()},'complete':0}
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
for k,wire in enumerate(cases):
    weapon,=struct.unpack_from('<i',wire,20);local,first,second,alternate,base,mode,mode_weapon=struct.unpack_from('<I4iIi',wire,24)
    before=bytearray([0xa5])*0x2000
    before[0x34:0x3c]=wire[:8];before[0x1080:0x1084]=wire[8:12];before[0xf80:0xf88]=wire[12:20]
    u.mem_write(player,bytes(before));put(0x7c75d4,'<I',player if local else 0)
    for a,v in [(0x85ccd8,first),(0x85cd00,second),(0x85cce0,alternate),(0x87210c,base),(0x7cabc4,mode_weapon)]:put(a,'<i',v)
    put(0x7cabd4,'<B',mode&255)
    for i in range(64):
        nonempty,model,resource=struct.unpack_from('<IIi',wire,52+i*12);desc=0x85cd08+i*1360
        put(desc+0x44,'<I',strings if nonempty else strings+32 if i%2 else 0)
        put(desc+0x48,'<I',model);put(desc+0x64,'<i',resource)
    for i in range(32):u.mem_write(0x7c71b0+i*16,wire[820+i*12:832+i*12])
    stop=stack+65000;put(stack+64000,'<IIi',stop,player,weapon);u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x4ae0d0,stop,count=100000);end=u.reg_read(UC_X86_REG_EIP);assert end==stop or end in boundaries,hex(end)
    actual=bytes(u.mem_read(player,0x2000));state=actual[0x34:0x3c]+actual[0x1080:0x1084]+actual[0xf80:0xf88]
    status=0 if end==stop else -3;model=u.reg_read(UC_X86_REG_EAX) if end==stop else 0xdeadbeef
    expected=struct.pack('<i',status)+state+struct.pack('<I',model)
    assert out[k*28:k*28+28]==expected,(k,hex(end),expected.hex(),out[k*28:k*28+28].hex())
    for off,n in [(0x34,8),(0x1080,4),(0xf80,8)]:before[off:off+n]=actual[off:off+n]
    assert actual==before,'unrelated player mutation'
    coverage['complete' if end==stop else boundaries[end]]+=1
assert all(coverage.values()),coverage
report=dict(result='PASS',cases=len(cases),coverage=coverage,scope='Whole 4ae0d0 with unchanged string length, timer clear and mode query; observation stops before clear/resource/mode adapters and missing-model assertion; loaded opaque model tokens, no asset creation or rendering')
(root/'artifacts/weapon-presentation-verification.json').write_text(json.dumps(report,indent=2));print(report)

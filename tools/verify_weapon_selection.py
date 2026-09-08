"""Original selection tail, with observation stops before unavailable adapters."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
player,entity,inventory,stack=[0x30000000+i*0x100000 for i in range(4)]
for a in (player,entity,inventory,stack):u.mem_map(a,65536)
boundaries={0x4a4cf5:'message',0x4aa0b0:'apply',0x4a4db4:'complete'}
for a in boundaries:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=a,end=a)
rng=random.Random(0x4a4c91);cases=[]
for k in range(4000):
    requested=rng.choice([-1,0,1,7,31,32,63,64]);first=rng.choice([requested,0]);second=rng.choice([-1,1,7,32,63,64])
    pending=rng.choice([-99,requested,second]);deadline=rng.randrange(-2147483648,2147483648)
    primary=rng.choice([-1,requested,second]);secondary=rng.choice([-1,requested,second])
    inp=struct.pack('<6i4I',requested,first,second,rng.choice([0,16,32,64]),primary,secondary,
        rng.choice([0,0xffffffff,1,0x80000000]),rng.choice([0,16,255]),
        rng.choice([0,1,256]),rng.choice([0,1,256]))
    extra=struct.pack('<4Bi',rng.choice([0,1,255]),rng.randrange(256),0xa5,0x5a,rng.randrange(-2147483648,2147483648))
    owned=bytes(rng.choice([0,0,1,255]) for _ in range(64))
    flags=struct.pack('<64I',*[rng.choice([0,0,0x40000]) for _ in range(64)])
    cases.append(struct.pack('<2i',pending,deadline)+extra+inp+owned+flags+struct.pack('<I',rng.choice([0,32,64])))
out=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--selection'],input=b''.join(cases))
assert len(out)==20*len(cases)
coverage={name:0 for name in boundaries.values()};queued=0;unowned_followup=0;cleared=0
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
for k,wire in enumerate(cases):
    pending,deadline=struct.unpack_from('<2i',wire)
    requested,first,second,split,primary,secondary,mask,pflags,force,defer=struct.unpack_from('<6i4I',wire,16)
    before=bytearray([0xa5])*0x1000
    struct.pack_into('<i',before,0xf80,pending);struct.pack_into('<i',before,0xb8,deadline)
    struct.pack_into('<I',before,0x10,pflags);before[0xf94:0xf9c]=wire[8:16]
    u.mem_write(player,bytes(before));put(entity+0x1428,'<I',mask)
    put(inventory+4,'<ii',primary,secondary);u.mem_write(inventory+0x18c,wire[56:120])
    for i in range(64):u.mem_write(0x85cf6c+i*1360,wire[120+4*i:124+4*i])
    u.mem_write(0x872448,wire[376:380]);put(0x85ccd8,'<i',first);put(0x85cd00,'<i',second);put(0x87211c,'<i',split)
    esp=stack+64000;put(esp+0x30,'<II',defer,force)
    for reg,value in [(UC_X86_REG_ESP,esp),(UC_X86_REG_EBX,player),(UC_X86_REG_EDI,entity),(UC_X86_REG_EBP,inventory),(UC_X86_REG_ESI,requested)]:u.reg_write(reg,value)
    u.emu_start(0x4a4c91,0,count=10000)
    end=u.reg_read(UC_X86_REG_EIP);assert end in boundaries,hex(end)
    result=bytes(u.mem_read(player,0x1000));actual=(struct.unpack_from('<i',result,0xf80)[0],struct.unpack_from('<i',result,0xb8)[0])
    expected_status=0 if end==0x4a4db4 else -3
    assert struct.unpack_from('<3i',out,k*20)==(expected_status,*actual),(k,hex(end),actual,struct.unpack_from('<3i',out,k*20))
    assert out[k*20+12:k*20+20]==result[0xf94:0xf9c],(k,'followup state')
    changed=result[0xf94:0xf9c]!=wire[8:16]
    before[0xf94:0xf96]=result[0xf94:0xf96];before[0xf98:0xf9c]=result[0xf98:0xf9c]
    struct.pack_into('<i',before,0xf80,actual[0]);struct.pack_into('<i',before,0xb8,actual[1]);assert result==before
    coverage[boundaries[end]]+=1
    queued+=actual!=(pending,deadline)
    unowned_followup+=changed and actual==(pending,deadline)
    cleared+=changed
assert all(coverage.values()) and queued and unowned_followup
report=dict(result='PASS',cases=len(cases),coverage=coverage,queued=queued,followup_clears=cleared,followup_without_queue=unowned_followup,
    scope='4a4c91..4a4db4 with unchanged paired-bit, ownership, descriptor, local-player, queue/timer, followup-byte and followup-clear callees; observation stops before message/apply; earlier selection gates and successful external adapters excluded')
(root/'artifacts/weapon-selection-verification.json').write_text(json.dumps(report,indent=2));print(report)

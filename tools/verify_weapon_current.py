"""Current-weapon resolution with unchanged original lookup and presentation gates."""
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
player,entities,stack=[0x30000000+i*0x100000 for i in range(3)]
for a in (player,entities,stack):u.mem_map(a,65536)
local=[False];presentations=[]
def presentation(uc,a,size,data):
    presentations.append(struct.unpack('<i',bytes(uc.mem_read(uc.reg_read(UC_X86_REG_ESP)+8,4)))[0])
    if local[0]:uc.emu_stop()
u.hook_add(UC_HOOK_CODE,presentation,begin=0x4ae0d0,end=0x4ae0d0)
rng=random.Random(0x4a5910);cases=[]
for k in range(3000):
    slot=rng.choice([0,1,1023]);handle=rng.choice([slot,slot+65536,slot-2147483648])
    slot2=(slot+1)%1024;handle2=slot2+65536
    query=rng.choice([handle,handle,handle,-1,handle+65536,1024])
    nodes=[(slot,handle,rng.choice([0,0,1]),0,rng.choice([-1,handle2,handle2,handle2+65536]),rng.choice([-2,-1,0,1,63,64])),
        (slot2,handle2,rng.choice([0,0,1]),rng.choice([0,1,4]),-1,rng.choice([-2,-1,0,1,63,64]))]
    cases.append(struct.pack('<iI12i',query,rng.randrange(2),*(v for n in nodes for v in n)))
out=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--current'],input=b''.join(cases));assert len(out)==8*len(cases)
complete=boundary=nonlocal_presentations=0
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
for k,wire in enumerate(cases):
    query,is_local=struct.unpack_from('<iI',wire);local[0]=bool(is_local);presentations.clear()
    u.mem_write(0x7394cc,bytes(4096));put(player+0x14,'<i',query);put(0x7c75d4,'<I',player if is_local else 0)
    for i in range(2):
        slot,handle,typ,kind,linked,weapon=struct.unpack_from('<6i',wire,8+i*24);p=entities+i*0x2000;info=entities+0x8000+i*0x2000
        put(0x7394cc+slot*4,'<I',p);put(p+0x2c,'<i',handle);put(p+0x24,'<i',typ)
        put(p+0x294,'<I',info);put(info+0x1b4,'<i',kind);put(p+0x200,'<i',linked);put(p+0x2a4,'<i',weapon)
    before=bytes(u.mem_read(player,0x2000))+bytes(u.mem_read(entities,65536))
    stop=stack+65000;put(stack+64000,'<II',stop,player);u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x4a5910,stop,count=10000);end=u.reg_read(UC_X86_REG_EIP)
    if end==stop:
        result=u.reg_read(UC_X86_REG_EAX);result=result if result<2147483648 else result-4294967296
        expected=(0,result);complete+=1;nonlocal_presentations+=len(presentations)
    else:
        assert end==0x4ae0d0 and is_local
        expected=(-3,-99);boundary+=1
    assert struct.unpack_from('<2i',out,k*8)==expected,(k,wire.hex(),expected,struct.unpack_from('<2i',out,k*8))
    assert before==bytes(u.mem_read(player,0x2000))+bytes(u.mem_read(entities,65536))
assert complete and boundary and nonlocal_presentations
report=dict(result='PASS',cases=len(cases),complete=complete,presentation_boundaries=boundary,complete_nonlocal_presentation_calls=nonlocal_presentations,
    scope='Complete unchanged 4a5910, entity lookup and linked-class predicates; unchanged 4ae0d0 nonlocal returns; local non--1 presentation stopped before entry, model updates excluded')
(root/'artifacts/weapon-current-verification.json').write_text(json.dumps(report,indent=2));print(report)

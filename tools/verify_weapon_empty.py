"""Verify empty-weapon decisions after original current-weapon resolution."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,player,stack,projectile=[0x30000000+i*0x100000 for i in range(4)]
for a in (entity,player,stack,projectile):u.mem_map(a,65536)
link=entity+0x4000;info=entity+0x8000;linkinfo=entity+0x9000
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
actions={0x4a4e80:1,0x4383c0:2,0x4a4a50:3}
for a in actions:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=a,end=a)
rng=random.Random(0x4a6f41);cases=[]
for k in range(4000):
    inventories=[]
    for _ in range(2):
        inventories.append(bytes(rng.choice([0,1,255]) for _ in range(64))+struct.pack('<96i',*[rng.choice([0,0,0,1,-1,2147483647]) for _ in range(96)]))
    supply=b''.join(struct.pack('<iiI',rng.randrange(-1,32),rng.choice([0,1,10,10]),rng.choice([0,0x100])) for _ in range(64))
    flags=struct.pack('<64I',*[rng.choice([0,0,0,0x20]) for _ in range(64)]);count=rng.choice([0,32,64])
    preference=struct.pack('<32i',*[rng.randrange(-1,65) for _ in range(32)])
    current=rng.randrange(8);linked=rng.randrange(2);passenger=rng.randrange(2) if linked else 0
    data=struct.pack('<6i7Ii',current,rng.choice([-1,current]),rng.choice([-1,current]),rng.choice([-1,7]),0,1,
        rng.choice([0,1,1,256]),rng.choice([0,0,1,256]),passenger,rng.randrange(2),rng.choice([0,1,256]),rng.choice([0,1,256]),linked,rng.choice([0,1,4]))
    cases.append(b''.join(inventories)+supply+flags+struct.pack('<I',count)+preference+data)
out=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--empty'],input=b''.join(cases));assert len(out)==len(cases)*12
coverage={0:0,1:0,2:0,3:0}
for k,wire in enumerate(cases):
    current,always,block,excluded,first,second,automatic,request,passenger,special,override,defer,linked,kind=struct.unpack_from('<6i7Ii',wire,2052)
    for p,inv in [(entity,wire[:448]),(link,wire[448:896])]:
        u.mem_write(p+0x42c,inv[:64]);u.mem_write(p+0x2ac,inv[64:192]);u.mem_write(p+0x32c,inv[192:]);put(p+0x24,'<I',0)
    put(entity+0x2c,'<i',7);put(link+0x2c,'<i',8);put(0x7394cc+28,'<II',entity,link)
    put(entity+0x294,'<I',info);put(link+0x294,'<I',linkinfo);put(linkinfo+0x1b4,'<i',kind)
    put(entity+0x200,'<i',8 if linked else -1);put(linkinfo+0x724,'<I',0x400000 if passenger else 0)
    put(link+0x8d4,'<I',link+0x2000);put(link+0x2004,'<I',link+0x2100);put(link+0x2104,'<i',7)
    put(player+0x14,'<i',7);put(player+0xf40,'<BB',automatic&255,defer&255);u.mem_write(player+0x1154,wire[1924:2052])
    for i in range(64):
        ammo,capacity,f268=struct.unpack_from('<iiI',wire,896+i*12);f264,=struct.unpack_from('<I',wire,1664+i*4)
        put(0x85cd08+i*1360+0x24,'<i',ammo);put(0x85cd08+i*1360+0x260,'<iII',capacity,f264,f268)
    count,=struct.unpack_from('<I',wire,1920);put(0x872448,'<I',count)
    for a,v in [(0x872118,always),(0x85cce0,block),(0x87210c,excluded),(0x85ccd8,first),(0x85cd00,second)]:put(a,'<i',v)
    put(0x64ecb9,'<B',override&255);put(0x8723b4,'<I',projectile if special else 0x872128)
    put(projectile+0x298,'<i',excluded);put(projectile+0x30,'<i',7);put(projectile+0x29c,'<f',1);put(projectile+0x34,'<f',1);put(projectile+0x7c,'<I',0);put(projectile+0x28c,'<I',0x872128)
    stop=stack+65000;put(stack+64000,'<7I',0,0,0,0,stop,player,request)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_EBP,entity);u.reg_write(UC_X86_REG_ESI,current);u.reg_write(UC_X86_REG_EDI,player)
    u.emu_start(0x4a6f41,stop,count=100000);end=u.reg_read(UC_X86_REG_EIP);assert end==stop or end in actions,hex(end)
    action=actions.get(end,0);selected=-1;esp=u.reg_read(UC_X86_REG_ESP)
    if action==3:
        _,selected,a,b=struct.unpack('<4i',bytes(u.mem_read(esp+4,16)));assert (a,b)==(1,0)
    elif action==1:assert struct.unpack('<3I',bytes(u.mem_read(esp+4,12)))==(player,1,1)
    elif action==2:assert struct.unpack('<4I',bytes(u.mem_read(esp+4,16)))==(0x5a05e0,0,0,0)
    assert struct.unpack_from('<3i',out,k*12)==(0,action,selected),(k,action,selected,struct.unpack_from('<3i',out,k*12))
    coverage[action]+=1
assert all(coverage.values()),coverage
report=dict(result='PASS',cases=len(cases),coverage=coverage,scope='Original 4a6f41..4a70db after resolved current weapon, with unmodified passenger/projectile/linked/ammo/replacement callees and observation stops at final external actions; current-weapon presentation update and actual firing/selection/message operations excluded')
(root/'artifacts/weapon-empty-verification.json').write_text(json.dumps(report,indent=2));print(report)

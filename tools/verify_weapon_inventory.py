"""Compare reserve lookup and preferred available weapon with original x86."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,player,stack=[0x30000000+i*0x100000 for i in range(3)]
for a in (entity,player,stack):u.mem_map(a,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
def call(a,args):
    stop=stack+65000;put(stack+64000,'<'+('I'*(len(args)+1)),stop,*[v&0xffffffff for v in args]);u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(a,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    return struct.unpack('<i',struct.pack('<I',u.reg_read(UC_X86_REG_EAX)))[0]
rng=random.Random(0x4a6e50);cases=[]
for k in range(4000):
    header=struct.pack('<IIi',int(k%13!=0),rng.choice([0,1,2,255,256]),rng.randrange(-2,64))
    owned=bytes([0]*64) if k%17==0 else bytes(rng.choice([0,0,0,1,255]) for _ in range(64))
    amounts=[rng.choice([-2147483648,-1,0,0,1,99,2147483647]) for _ in range(96)]
    inventory=owned+struct.pack('<96i',*amounts)
    supply=b''.join(struct.pack('<iiI',rng.randrange(-2,32),rng.choice([-1,0,1,30]),rng.choice([0,0x100,0x101])) for _ in range(64))
    preference=struct.pack('<32i',*[rng.randrange(-2,66) for _ in range(32)])
    cases.append(header+inventory+supply+preference)
run=subprocess.run([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--inventory'],input=b''.join(cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*16
selected_count=0
for k,wire in enumerate(cases):
    present,defer,weapon=struct.unpack_from('<IIi',wire);inventory=wire[12:460];supply=wire[460:1228]
    put(entity+0x2c,'<i',7);put(entity+0x24,'<i',0);put(0x7394cc+28,'<I',entity);put(player+0x14,'<i',7 if present else -1)
    u.mem_write(entity+0x42c,inventory[:64]);u.mem_write(entity+0x2ac,inventory[64:192]);u.mem_write(entity+0x32c,inventory[192:])
    u.mem_write(player+0x1154,wire[1228:]);put(player+0xf41,'<B',defer&255)
    for i in range(64):
        ammo,capacity,flags=struct.unpack_from('<iiI',supply,i*12)
        put(0x85cd08+i*1360+0x24,'<i',ammo);put(0x85cd08+i*1360+0x260,'<i',capacity);put(0x85cd08+i*1360+0x268,'<I',flags)
    amount=call(0x42add0,[entity if present else 0,weapon]);selected=call(0x4a6e50,[player])
    expected=(0,amount,0,selected);actual=struct.unpack_from('<4i',run.stdout,k*16)
    assert actual==expected,(k,actual,expected)
    selected_count+=selected>=0
assert 0<selected_count<len(cases)
# Bounds extensions: original has no upper weapon or ammo-type check here.
wire=bytearray(cases[1]);struct.pack_into('<i',wire,8,64)
out=struct.unpack('<4i',subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--inventory'],input=wire))
assert out[:2]==(-4,-99)
wire=bytearray(cases[1]);struct.pack_into('<i',wire,8,0);struct.pack_into('<i',wire,460,32)
out=struct.unpack('<4i',subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--inventory'],input=wire))
assert out[:2]==(-4,-99)
report=dict(result='PASS',cases=len(cases),selected=selected_count,bounds_rejections=2,scope='Complete original 42add0 and 4a6e50 with unchanged ownership, ammo and flag predicates, 32-entry preference order, null entities and 32-bit wrapping totals; weapon switching, presentation and full empty-weapon handler excluded')
(root/'artifacts/weapon-inventory-verification.json').write_text(json.dumps(report,indent=2));print(report)

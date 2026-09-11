"""SP lethal attribution prefix with real burn-source and ordered UID lookup."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');u.mem_write(0x64ecb9,bytes(2))
entry=int(re.search(r'_rf_entity_damage_credit_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x41a44d);cases=[];expected=[]
for n in range(4096):
    health=(-1,-.1,-0.0,0.0,.5,100)[n%6];kind=(4,4,4,0,7,-1)[n//6%6]
    source=rng.choice([0xffffffff,0xffffffff,0x12340001]);aux=rng.choice([-1,26,99,100]);burn=rng.choice([0,0,1,2]);burn_source=rng.choice([0xffffffff,0x34560003])
    count=n%5;targets=[(rng.choice([26,99,26,-1]),0x23450000+i) for i in range(4)]
    wire=struct.pack('<f',health)+w(0xaabbccdd,burn,burn_source,kind,source,aux,count,*[v for t in targets for v in t]);cases.append(wire)
    obj=bytearray(0x1500);obj[0x34:0x38]=wire[:4];obj[0x13d8:0x13dc]=w(b+0x4000 if burn else 0);obj[0x144c:0x1450]=wire[4:8];u.mem_write(b,bytes(obj));u.mem_write(b+0x4034,w(burn_source))
    u.mem_write(0x5cb2ec,w(b+0x6000 if count else 0x5cb060))
    for i,(uid,handle) in enumerate(targets):
        ptr=b+0x6000+i*0x300;u.mem_write(ptr+0x20,w(uid));u.mem_write(ptr+0x2c,w(handle));u.mem_write(ptr+0x28c,w(ptr+0x300 if i+1<count else 0x5cb060))
    u.mem_write(stack+0x24,w(source));u.mem_write(stack+0x2c,w(aux));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EBP,kind&0xffffffff);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x41a44d,0x41a505,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41a505
    result=bytes(u.mem_read(b+0x144c,4));expected.append(w(0)+wire[:4]+result+wire[8:16])
for bits in (0x7fc00000,0x7f800000):
    wire=bytearray(cases[0]);wire[:4]=w(bits);cases.append(bytes(wire));expected.append(w(-2)+wire[:16])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--damage-credit'],input=b''.join(cases));assert len(actual)==len(cases)*20
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*20:(i+1)*20]==want,('PC',i,wire.hex(),actual[i*20:(i+1)*20].hex(),want.hex())
    x.mem_write(b,wire[:16]);x.mem_write(b+0x1000,wire[32:]);x.mem_write(stack,w(stop,b)+wire[16:28]+w(b+0x1000)+wire[28:32])
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,16));assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(result='PASS',original_cases=4096,port_guards=2,pc_nxdk_cases=len(cases),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='SP41a44d..41a505 with complete unmodified42f5a0 and425210; no hooks. Exact attribution on PC/NXDK, including existing burn source, auxiliary UID first-match order, missing UID, explicit source, positive health preservation and signed zero. Synthetic entity list/owned burn object; no live lifecycle, scoring, death or multiplayer.')
(root/'artifacts/damage-credit.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

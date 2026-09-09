"""Original stance decision boundaries, with eligibility supplied by caller."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);entity=base;esp=base+32000
boundaries=(0x41f760,0x41f949,0x41f79b,0x41f7b8,0x41f7c1)
for address in boundaries:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,user:uc.emu_stop(),begin=address,end=address)
rng=random.Random(0x41f743);cases=[];expected=[];effects=[0,0,0];guards=0
for n in range(2400):
    special=rng.randrange(23);eligible=rng.randrange(2);flags=rng.choice([0,0x400,0x80400])
    duration=rng.choice([0,.25,1]);age=duration*rng.choice([0,.25,.5,1])
    current=rng.choice([special,rng.randrange(23)]);next_state=rng.choice([special,rng.randrange(23)]) if duration else -1
    controller=struct.pack('<iiffiI',current,next_state,duration,age,0,0)
    motions=[rng.choice([-1]+list(range(24))) for _ in range(23)]
    if n%61==0:special=23
    if n%67==0:eligible=2
    if n%71==0:controller=controller[:8]+struct.pack('<f',math.nan)+controller[12:]
    cases.append(controller+struct.pack('<23iiII',*motions,special,eligible,flags))
    if special==23 or eligible==2 or not math.isfinite(struct.unpack_from('<f',controller,8)[0]):
        status=-4 if special==23 or eligible==2 else -2
        expected.append(struct.pack('<iiI',status,-99,0xa5a5a5a5)+controller);guards+=1;continue
    u.mem_write(entity+0x138c,controller[:16]);u.mem_write(entity+0x810,struct.pack('<I',flags))
    for i,motion in enumerate(motions):u.mem_write(entity+0x8e4+16*i,struct.pack('<i',motion))
    u.reg_write(UC_X86_REG_ESI,entity);u.reg_write(UC_X86_REG_EDI,special);u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41f743 if eligible else 0x41f7ab,0,count=10000)
    endpoint=u.reg_read(UC_X86_REG_EIP);assert endpoint in boundaries
    effect=1 if endpoint==0x41f79b else 2 if endpoint==0x41f7b8 else 0;effects[effect]+=1
    expected.append(struct.pack('<iiI',0,eligible,effect)+bytes(u.mem_read(entity+0x138c,16))+controller[16:])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_motion_probe.exe'),'--stance'],input=b''.join(cases))
assert len(actual)==len(expected)*36
for n,want in enumerate(expected):assert actual[n*36:(n+1)*36]==want,(n,actual[n*36:(n+1)*36].hex(),want.hex())
report=dict(result='PASS',original_cases=sum(effects),none=effects[0],crouch=effects[1],stand=effects[2],port_guards=guards,
    scope='Original 41f743 stance gate or 41f7ab ineligible branch, unchanged membership/request and physical-crouch query. Stops before collider-changing calls or return/fallthrough; eligibility predicates and physics effects excluded.')
(root/'artifacts/motion-stance-verification.json').write_text(json.dumps(report,indent=2));print(report)

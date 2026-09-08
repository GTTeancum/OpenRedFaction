"""Verify pre-candidate timer/reset gate against original instructions."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,stack=0x30000000,0x30100000
for a in (entity,stack):u.mem_map(a,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
observed=[]
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:observed.append(struct.unpack('<i',bytes(uc.mem_read(entity+0x744,4)))[0]),begin=0x41ae70,end=0x41ae70)
rng=random.Random(0x41f5ae);cases=[]
for k in range(5000):
    pending=rng.choice([-1,-1,-1,0]);behavior=rng.choice([0,1,1,3,3]);mode=rng.choice([0,0,0,9,11,12,13,15,16])
    weapon=rng.choice([-2,-1,0,1,31,63,64,65]);network=rng.choice([0,0,0,1,256]);override=rng.choice([0,0,0,1,256])
    now=rng.choice([0,1072800000,1072800000-800,1072800000-799,rng.randrange(1072800001)])
    count=rng.choice([0,1,32,64]);flags=[rng.getrandbits(32) for _ in range(64)];deadline=rng.choice([-1,0,99,1072800000])
    context=struct.pack('<I6fifIi',0,6,.3,1.5,20,7,9,-1,1,override,now)
    actor=bytes(60)+struct.pack('<iI3iI6f3I',mode,0,weapon,0,behavior,network,0,0,0,0,0,0,0,0,0)
    cases.append(struct.pack('<i',pending)+context+actor+struct.pack('<I64IiiI',count,*flags,deadline,0,1))
run=subprocess.run([str(root/'build/pc/Release/rf_turn_probe.exe'),'--prepare'],input=b''.join(cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*16
calls=0;chosen=None
for k,wire in enumerate(cases):
    pending,=struct.unpack_from('<i',wire);override,now=struct.unpack_from('<Ii',wire,40)
    mode,_,weapon,_,behavior,network=struct.unpack_from('<iI3iI',wire,108)
    count,=struct.unpack_from('<I',wire,168);flags=struct.unpack_from('<64I',wire,172);deadline,=struct.unpack_from('<i',wire,428)
    put(entity+0x554,'<i',behavior);put(entity+0x6cc,'<i',pending);put(entity+0x858,'<I',entity+0x4000);put(entity+0x4004,'<i',mode)
    put(entity+0x2a4,'<i',weapon);put(entity+0x2c,'<i',-1);put(entity+0x744,'<i',deadline)
    put(0x6fc4d8,'<B',network&255);put(0x64ecb9,'<B',override&255);put(0x5a3ed8,'<i',now);put(0x872448,'<I',count)
    for i,flags_word in enumerate(flags):put(0x85cf6c+i*1360,'<I',flags_word)
    observed.clear();u.reg_write(UC_X86_REG_ESI,entity);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41f5ae,0x41f61d,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41f61d
    actual=struct.unpack_from('<4i',run.stdout,k*16)
    final,=struct.unpack('<i',bytes(u.mem_read(entity+0x744,4)))
    expected=(0,final,len(observed),observed[0] if observed else -99)
    assert actual==expected,(k,actual,expected)
    if observed:chosen=wire;calls+=1
assert chosen is not None and 0<calls<len(cases)
# Missing adapter must not set the deadline; callback failure follows its write.
missing=chosen[:-4]+struct.pack('<I',0)
out=struct.unpack('<4i',subprocess.check_output([str(root/'build/pc/Release/rf_turn_probe.exe'),'--prepare'],input=missing))
assert out==(-3,struct.unpack_from('<i',chosen,428)[0],0,-99),out
failure=chosen[:432]+struct.pack('<iI',-2,1)
out=struct.unpack('<4i',subprocess.check_output([str(root/'build/pc/Release/rf_turn_probe.exe'),'--prepare'],input=failure))
assert out[0]==-2 and out[2]==1 and out[1]==out[3],out
report=dict(result='PASS',cases=len(cases),reset_calls=calls,adapter_error_cases=2,scope='Original 41f5ae..41f61c including unmodified mode, weapon-flag and timer callees; reset enters original 41ae70 with absent entity handle or invalid weapon index; populated reset effects excluded; callback observes deadline written before invocation')
(root/'artifacts/locomotion-prepare-verification.json').write_text(json.dumps(report,indent=2));print(report)

"""Verify crouch eligibility against original code up to visibility work."""
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
base=0x30000000;u.mem_map(base,65536);entity=base;modeptr=base+8192;target=base+12288;stack=base+60000;stop=base+64000
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=0x429b99,end=0x429b99)
def put(a,fmt,*values):u.mem_write(a,struct.pack(fmt,*values))
def run(address):
    put(stack,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(address,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP) in (stop,0x429b99)
    return u.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(0x429ae0);cases=[];expected=[];counts=[0,0,0];guards=0
for n in range(9000):
    fields=[rng.choice([-1,0,8,20]),rng.randrange(8),2,5,rng.choice([0,1,2,9,11,12,13,15,18]),rng.choice([3,3,12,12,0,7]),rng.randrange(4),rng.randrange(3),rng.choice([0,1,2,11,12]),rng.randrange(3),rng.choice([0,0x4000,0x2000000,0x2004000]),rng.choice([0,0,1,255]),rng.choice([0,0,1,128]),rng.randrange(2)]
    if n%71==0:fields[11]=256
    if n%73==0:fields[13]=2
    cases.append(struct.pack('<10i4I',*fields))
    if fields[11]>255 or fields[13]>1:
        expected.append(struct.pack('<iI',-4,0xa5a5a5a5));guards+=1;continue
    motion,weapon,ex0,ex1,mode,action,state,stance,behavior,prop,flags,g0,g1,present=fields
    for off,val in [(0x974,motion),(0x2a4,weapon),(0x520,action),(0x740,state),(0x7bc,stance),(0x554,behavior),(0x4ec,prop),(0x7d0,flags),(0x560,6 if present else -1)]:put(entity+off,'<i',val)
    put(entity+0x858,'<I',modeptr);put(modeptr+4,'<i',mode)
    put(0x872118,'<i',ex0);put(0x872468,'<i',ex1);put(0x64ecb9,'<B',g0);put(0x6fc4d8,'<B',g1)
    put(0x7394cc+6*4,'<I',target);put(target+0x2c,'<i',6)
    result=0
    if run(0x402ab0):
        result=run(0x429ae0)
        if u.reg_read(UC_X86_REG_EIP)==0x429b99:result=2
        else:assert result in (0,1)
    counts[result]+=1;expected.append(struct.pack('<iI',0,result))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_motion_probe.exe'),'--crouch-eligibility'],input=b''.join(cases))
assert len(actual)==len(expected)*8
for n,want in enumerate(expected):assert actual[n*8:(n+1)*8]==want,(n,cases[n].hex(),actual[n*8:(n+1)*8].hex(),want.hex())
assert all(counts)
report=dict(result='PASS',original_cases=sum(counts),denied=counts[0],allowed=counts[1],pending_visibility=counts[2],port_guards=guards,scope='Unmodified 402ab0 and 429ae0, including mode predicate and object lookup; stops at 429b99 before target eye and visibility work. Does not verify collision or ray results.')
(root/'artifacts/motion-crouch-eligibility-verification.json').write_text(json.dumps(report,indent=2));print(report)

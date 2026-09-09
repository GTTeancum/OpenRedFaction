"""Compare clip-pool reset, allocation, release and first exhaustion."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;stop=base+256
u.mem_write(0x1d002d0,b'\xa5'*(48*48));rng=random.Random(5496);commands=[];expected=[];live=[];exhaustions=0
def call(address,arg=0):
    u.mem_write(esp,struct.pack('<II',stop,arg));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(address,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
def step(kind,index=0):
    global exhaustions
    commands.append(struct.pack('<II',kind,index));slot=99;status=0
    if kind==0:call(0x549270);live.clear()
    elif kind==1:
        call(0x5496e0);pointer=u.reg_read(UC_X86_REG_EAX)
        if pointer:slot=(pointer-0x1d002d0)//48;live.append(slot)
        else:status=-4;exhaustions+=1
    else:call(0x5492d0,0x1d002d0+index*48);live.remove(index)
    used=struct.unpack('<I',u.mem_read(0x1d02610,4))[0]
    order=[(x-0x1d002d0)//48 for x in struct.unpack('<48I',u.mem_read(0x1d02550,192))]
    expected.append(struct.pack('<iII48I',status,slot,used,*order)+bytes(u.mem_read(0x1d002d0,2304)))
step(0)
for cycle in range(10):
    for _ in range(200):
        if live and (len(live)>=46 or rng.randrange(3)==0):step(2,rng.choice(live))
        else:step(1)
    while len(live)<47:step(1)
    step(1);step(0)
probe=str(root/'build/pc/Release/rf_model_probe.exe');actual=subprocess.check_output([probe,'--clip-pool'],input=b''.join(commands))
assert actual==b''.join(expected)
# Port-only invalid releases and repeated post-exhaustion allocation are atomic.
for command in [(2,0),(2,48)]:
    out=subprocess.check_output([probe,'--clip-pool'],input=struct.pack('<4I',0,0,*command))
    assert out[2508:2512]==struct.pack('<i',-4) and out[2512:]==out[4:2508]
out=subprocess.check_output([probe,'--clip-pool'],input=struct.pack('<II',1,0)*49)
assert out[-2508:]==out[-5016:-2508]
report=dict(result='PASS',operations=len(commands),first_exhaustions=exhaustions,port_guard_cases=3,
    scope='Complete unchanged 0x549270/0x5496e0/0x5492d0 over randomized valid lifetimes and first exhaustion; normalized slot order, counter and all record bytes exact; port rejects invalid release and post-exhaustion calls')
(root/'artifacts/model-clip-pool-verification.json').write_text(json.dumps(report,indent=2));print(report)

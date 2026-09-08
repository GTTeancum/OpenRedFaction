"""Verify turn direction gate and transform against unmodified original math."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,stack,stop=0x30000000,0x30100000,0x30200000
for address in (entity,stack,stop):u.mem_map(address,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
stops=(0x41fb3e,0x41fc84,0x41fd2a)
for address in stops:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=address,end=address)
rng=random.Random(0x41fa7c);cases=[]
for k in range(5000):
    vector=rng.choice([(0,0,0),(.1,0,0),(0,.1,0),tuple(rng.uniform(-10,10) for _ in range(3))])
    matrix=[rng.uniform(-1,1) for _ in range(9)] if k%3 else [1,0,0,0,1,0,0,0,1]
    cases.append(struct.pack('<IIi12f',rng.choice([0,4]),rng.choice([0,8]),rng.choice([0,1,2]),*vector,*matrix))
for sign in (-1,1):
    for small in (0,1e-18,1e-16,1e-10):
        cases.append(struct.pack('<IIi12f',0,8,1,1,small,0,1,0,0,0,1,0,sign*.5,sign*.8660254,0))
threshold=struct.unpack('<f',struct.pack('<f',.1))[0]
bits=struct.unpack('<I',struct.pack('<f',threshold))[0]
for step in range(1,41):
    x=struct.unpack('<f',struct.pack('<I',bits-step))[0]
    ybits=struct.unpack('<I',struct.pack('<f',(threshold*threshold-x*x)**.5))[0]
    for neighbor in range(-2,3):
        y=struct.unpack('<f',struct.pack('<I',ybits+neighbor))[0]
        cases.append(struct.pack('<IIi12f',0,8,1,x,y,0,1,0,0,0,1,0,0,0,1))
out=subprocess.run([str(root/'build/pc/Release/rf_turn_probe.exe'),'--direction'],input=b''.join(cases),capture_output=True,check=True).stdout
assert len(out)==20*len(cases)
for k,case in enumerate(cases):
    put(entity+0x294,'<I',entity+0x3000);u.mem_write(entity+0x3000+0x728,case[:4]);u.mem_write(entity+0x7d0,case[4:8]);u.mem_write(entity+0x588,case[8:12])
    u.mem_write(entity+0x7a0,case[12:24]);u.mem_write(entity+0x48,case[24:60]);u.mem_write(stack+62000,bytes(256))
    u.reg_write(UC_X86_REG_ESP,stack+62000);u.reg_write(UC_X86_REG_ESI,entity);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x41fa7c,stop,count=100000);eip=u.reg_read(UC_X86_REG_EIP);assert eip in stops
    eligible=int(eip==0x41fb3e)
    local=bytes(u.mem_read(stack+62016,12)) if eligible else bytes(12)
    expected=struct.pack('<i',0)+local+struct.pack('<I',eligible)
    assert out[k*20:(k+1)*20]==expected,(k,out[k*20:(k+1)*20].hex(),expected.hex())
report=dict(result='PASS',cases=len(cases),scope='Original 0x41fa7c direction gate through normalization, dot and local transform; observation stops before later candidate/reset decisions')
(root/'artifacts/turn-direction-verification.json').write_text(json.dumps(report,indent=2));print(report)

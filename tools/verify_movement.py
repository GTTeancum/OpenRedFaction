"""Verify complete movement setting routine 0x427450 against original x86."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,info,stack,stop=[0x30000000+i*0x100000 for i in range(4)]
for a in (entity,info,stack,stop):u.mem_map(a,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
def rd(a,n):return bytes(u.mem_read(a,n))
rng=random.Random(0x427450);cases=[]
for k in range(6000):
    number=lambda:2**rng.randrange(-20,21)*rng.uniform(.5,1)
    state=struct.pack('<ffi',rng.choice([-0.0,1,3000]),rng.uniform(1,20),rng.randrange(3))
    config=struct.pack('<I6f',rng.choice([0,0x800,0x400,0x840]),number(),rng.uniform(0,2),rng.uniform(0,2),number(),rng.uniform(1,10),rng.uniform(1,10))
    args=struct.pack('<iifI',rng.choice([-2147483648,-1,0,1,2,3,2147483647]),rng.choice([-1]*4+[-2,0,1]),number(),rng.choice([0,1,2,255]))
    cases.append(state+config+args)
out=subprocess.run([str(root/'build/pc/Release/rf_movement_probe.exe')],input=b''.join(cases),capture_output=True,check=True).stdout
assert len(out)==16*len(cases)
for k,case in enumerate(cases):
    u.mem_write(entity,bytes(65536));put(entity+0x294,'<I',info)
    u.mem_write(entity+0x8c,case[:4]);u.mem_write(entity+0x8c0,case[4:12]);u.mem_write(info+0x724,case[12:16]);u.mem_write(info+0x50,case[16:32])
    u.mem_write(0x594590,case[32:36]);u.mem_write(0x59458c,case[36:40]);u.mem_write(entity+0x75c,case[44:48]);u.mem_write(entity+0x98,case[48:52]);u.mem_write(0x64ecb9,case[52:53])
    before=bytearray(rd(entity,65536))
    put(stack+64000,'<I',stop);u.mem_write(stack+64004,case[40:44]);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ECX,entity);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x427450,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=struct.pack('<i',0)+rd(entity+0x8c,4)+rd(entity+0x8c0,8)
    assert out[k*16:(k+1)*16]==expected,(k,case.hex(),out[k*16:(k+1)*16].hex(),expected.hex())
    after=bytearray(rd(entity,65536));after[0x8c:0x90]=before[0x8c:0x90];after[0x8c0:0x8c8]=before[0x8c0:0x8c8];assert after==before
bad=[]
for offset,value in [(16,0.0),(20,float('nan')),(48,float('inf'))]:
    c=bytearray(cases[0]);struct.pack_into('<I',c,12,0x800);struct.pack_into('<ii',c,40,1,-1);struct.pack_into('<f',c,offset,value);bad.append(bytes(c))
out=subprocess.run([str(root/'build/pc/Release/rf_movement_probe.exe')],input=b''.join(bad),capture_output=True,check=True).stdout
for i,c in enumerate(bad):assert struct.unpack_from('<i',out,i*16)[0]!=0 and out[i*16+4:(i+1)*16]==c[:12]
report=dict(result='PASS',cases=len(cases),rejection_cases=len(bad),scope='Complete original 0x427450 and unmodified 0x40a210; exact response/speed/mode fields, unchanged remaining entity RAM, forced actions and configurable global overrides')
(root/'artifacts/movement-settings-verification.json').write_text(json.dumps(report,indent=2));print(report)

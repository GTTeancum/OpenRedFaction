"""Compare loop cursor and event block with original instructions, no hooks."""
import hashlib,json,struct,subprocess,sys,random,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBX,UC_X86_REG_EBP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motion,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motion,data,stack): u.mem_map(a,65536)
u.mem_write(desc+0xf5c,struct.pack('<I',motion)); u.mem_write(motion+0x78,struct.pack('<I',data))
rng=random.Random(0x51bc78); cases=[]
for i in range(3000):
    start=rng.randrange(-10000,10000); end=start+rng.randrange(1,20000)
    phase=struct.unpack('<f',struct.pack('<f',rng.choice([0,1,.5]) if i%3==0 else rng.random()))[0]
    cursor=start+math.floor(phase*(end-start)); previous=rng.randrange(start,end+1)
    markers=[rng.choice([start,end,previous,cursor,cursor-1,cursor+1,rng.randrange(start,end+1)]) for _ in range(2)]
    cases.append(struct.pack('<iif4i',start,end,phase,previous,*markers,i%2))
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--map-loop'],input=b''.join(cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*12
for i,raw in enumerate(cases):
    start,end,phase,previous,m0,m1,wrapped=struct.unpack('<iif4i',raw)
    u.mem_write(data+16,struct.pack('<2i',start,end)); u.mem_write(obj+0x1d04,struct.pack('<f',phase)); u.mem_write(obj+0x12d8,struct.pack('<i',previous)); u.mem_write(obj+0x1d44,bytes(8))
    u.mem_write(motion+0x50,struct.pack('<i',m0)); u.mem_write(motion+0x64,struct.pack('<i',m1))
    esp=stack+64000; u.mem_write(esp,bytes(128)); u.mem_write(esp+0x18,struct.pack('<I',desc+0xf5c)); u.mem_write(esp+0x24,struct.pack('<i',previous)); u.mem_write(esp+0x17,bytes([wrapped]))
    for register,value in [(UC_X86_REG_ESP,esp),(UC_X86_REG_ESI,obj),(UC_X86_REG_EDI,obj+0x12d8),(UC_X86_REG_EBP,0),(UC_X86_REG_EBX,desc+0xf5c),(UC_X86_REG_FPCW,0x37f)]: u.reg_write(register,value)
    u.emu_start(0x51bc78,0x51bddb,count=10000); assert u.reg_read(UC_X86_REG_EIP)==0x51bddb
    flags=u.mem_read(obj+0x1d44,2); expected=bytes(u.mem_read(obj+0x12d8,4))+struct.pack('<I',flags[0]|flags[1]<<1)
    status,=struct.unpack_from('<i',run.stdout,i*12)
    assert status==0 and run.stdout[i*12+4:(i+1)*12]==expected,(i,status,run.stdout[i*12+4:(i+1)*12].hex(),expected.hex())
report=dict(result='PASS',samples=len(cases),scope='Original dominant-loop cursor/event block including CRT floor; not full update')
(root/'artifacts/motion-loop-verification.json').write_text(json.dumps(report,indent=2)); print(report)

"""Verify mixed-loop branch non-loop advancement and primary selection."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EDX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motions,data,stack=[0x30000000+i*0x100000 for i in range(5)]
for a in (obj,desc,motions,data,stack): u.mem_map(a,65536)
u.mem_write(obj+0x1d50,struct.pack('<I',desc))
for i in range(16):
    u.mem_write(desc+0xf5c+i*4,struct.pack('<I',motions+i*256)); u.mem_write(motions+i*256+0x78,struct.pack('<I',data+i*256))
rng=random.Random(0x51bd20); cases=[]
for trial in range(3000):
    selected=rng.choice([-1,0,2]); candidate_flag=rng.randrange(2); primary_flag=0 if selected==2 else rng.randrange(2)
    def env(): return struct.pack('<f4i',rng.choice([0,.5,1,2]),160,9600,rng.randrange(1000),rng.randrange(1000))
    candidate=env(); primary=candidate if selected==2 else env()
    slots=b''.join(struct.pack('<iif',[4,6,9][i] if i<3 else i,rng.randrange(0,11000),rng.choice([0,.5,1])) for i in range(16))
    state=struct.pack('<I',3)+slots+struct.pack('<3i',0,selected,1)+struct.pack('<4I6f',0,1,123,456,1,2,3,4,5,6)
    cases.append(state+struct.pack('<Ii',2,rng.randrange(1000))+candidate+primary+struct.pack('<2i',candidate_flag,primary_flag))
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--advance-candidate'],input=b''.join(cases),capture_output=True,check=True)
assert len(run.stdout)==len(cases)*252
for k,raw in enumerate(cases):
    u.mem_write(obj+0x12d0,raw[:196]); u.mem_write(obj+0x1cfc,raw[196:204]); u.mem_write(obj+0x1d48,raw[204:208]); u.mem_write(obj+0x1d18,raw[216:248]); u.mem_write(obj+0x1d4c,b'\0'); u.mem_write(obj+0x1d14,b'\x01')
    for motion,env in [(9,raw[256:276]),(4,raw[276:296])]:
        u.mem_write(data+motion*256+16,env[4:12]); u.mem_write(data+motion*256+36,env[12:20]); u.mem_write(data+motion*256+80,struct.pack('<I',84)+env[:4])
    cf,pf=struct.unpack_from('<2i',raw,296); u.mem_write(desc+0x120c+2,bytes([cf]));u.mem_write(desc+0x120c+4,bytes([pf]));u.mem_write(desc+0x120c+9,b'\0')
    esp=stack+64000; u.mem_write(esp,bytes(128)); u.mem_write(esp+0x10,struct.pack('<I',2));u.mem_write(esp+0x18,struct.pack('<I',desc+0xf5c));u.mem_write(esp+0x2c,raw[252:256])
    for r,v in [(UC_X86_REG_ESP,esp),(UC_X86_REG_ESI,obj),(UC_X86_REG_EDI,obj+0x12d8+24),(UC_X86_REG_EBP,9),(UC_X86_REG_EDX,2),(UC_X86_REG_FPCW,0x37f)]:u.reg_write(r,v)
    u.emu_start(0x51bd20,0x51bddb,count=10000); assert u.reg_read(UC_X86_REG_EIP)==0x51bddb
    expected=struct.pack('<i',0)+bytes(u.mem_read(obj+0x12d0,196))+bytes(u.mem_read(obj+0x1cfc,8))+bytes(u.mem_read(obj+0x1d48,4))+raw[208:216]+bytes(u.mem_read(obj+0x1d18,32))
    assert run.stdout[k*252:(k+1)*252]==expected,k
report=dict(result='PASS',samples=len(cases),scope='Original mixed-loop non-loop candidate block with slot/motion IDs differing; no full update')
(root/'artifacts/motion-primary-verification.json').write_text(json.dumps(report,indent=2)); print(report)

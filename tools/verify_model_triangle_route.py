"""Compare triangle routing with the original branch before polygon generation."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;triangle=base+256
routes={0x52f890:0,0x52f85b:1,0x52f850:1,0x52f5b2:2}
def stop(u,address,size,user):
    if address in routes:u.emu_stop()
u.hook_add(UC_HOOK_CODE,stop)
rng=random.Random(52473);cases=[];expected=[];counts=[0,0,0]
for n in range(3000):
    records=[]
    for i in range(3):
        record=struct.pack('<6fBB6s',*(rng.randint(-100,100)/16 for _ in range(6)),rng.choice([0,0,1,2,4,8,16,32,128]),0,bytes(6))
        records.append(record);u.mem_write(0x1bf7000+i*12,record[:12]);u.mem_write(0x1c3d554+i,record[24:25])
    camera=[rng.randint(-100,100)/16 for _ in range(3)];rotation=[1,0,0,0,1,0,0,0,1]
    flags=[n%2,(n//2)%2,1,0,(n//4)%2];two_sided=32 if n%3 else 0
    view=struct.pack('<23f5I',*camera,*rotation,1,2,320,-240,320,240,0,0,640,480,100,*flags)
    cases.append(b''.join(records)+view+struct.pack('<4H',0,1,2,two_sided))
    u.mem_write(0x1818690,view[:12]);u.mem_write(0x18186e0,view[36:48]);u.mem_write(0x5a4d19,bytes([flags[0]]))
    u.mem_write(triangle,struct.pack('<4H',0,1,2,two_sided));u.mem_write(esp,bytes(1024));u.mem_write(esp+0x12,bytes([flags[1],flags[4]]))
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EDI,triangle)
    u.emu_start(0x52f473,0,count=10000);end=u.reg_read(UC_X86_REG_EIP);assert end in routes
    route=routes[end];counts[route]+=1;expected.append(struct.pack('<iI',0,route))
probe=str(root/'build/pc/Release/rf_model_probe.exe')
actual=subprocess.check_output([probe,'--triangle-route'],input=b''.join(cases));assert actual==b''.join(expected)
for index in (3,32768,65535):
    case=bytearray(cases[0]);struct.pack_into('<H',case,208,index)
    assert subprocess.check_output([probe,'--triangle-route'],input=case)==struct.pack('<iI',-4,99)
report=dict(result='PASS',cases=len(cases),rejected=counts[0],direct=counts[1],clip=counts[2],port_bounds_cases=3,
    scope='Unchanged renderer routing including full facing callees, stopping before index capacity checks or polygon generation; exact reject/direct/clip decisions, no clipping output or draw claim')
(root/'artifacts/model-triangle-route-verification.json').write_text(json.dumps(report,indent=2));print(report)

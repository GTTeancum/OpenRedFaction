"""Original generic factory identity/parent/transform overlay without assets."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x10000);parent=base+0x2000;params=base+0x4000;stack=base+0xe000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
calls=[]
def hook(cpu,address,size,data):
 if address not in (0x487100,0x4ffa80,0x49ec90):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(sp)
 if address==0x487100:
  assert [read(sp+4),read(sp+8)]==[kind,7];cpu.reg_write(UC_X86_REG_EAX,base);pop=4
 elif address==0x4ffa80:
  assert cpu.reg_read(UC_X86_REG_ECX)==base+0x18 and read(sp+4)==0x73db24;pop=8
 else:
  assert read(sp+4)==base+0x88 and read(sp+8)==params;pop=4
 calls.append(address);cpu.reg_write(UC_X86_REG_ESP,sp+pop);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
cases=0
position=struct.pack('<3f',1.25,-2.5,4.0);matrix=struct.pack('<9f',0,1,0,-1,0,0,0,0,1)
for kind in (0,4,5,6,8,9,10):
 for flags in (0,0x4000,0x10000,0x24008):
  for valid_parent in (False,True):
   for radius in (-1.0,0.0,2.5):
    u.mem_write(0x7394cc,bytes(4096));u.emu_start(0x486ce9,0x486d3e,count=100000)
    u.mem_write(0x708744,pack(1));u.mem_write(0x59f7e4,pack(100));u.mem_write(0x6460e8,pack(0))
    before=bytes([0xa5])*0x1494;u.mem_write(base,before);u.mem_write(parent,bytes(0x300))
    if valid_parent:
     u.mem_write(0x7394d0,pack(parent));u.mem_write(parent+0x2c,pack(0x43210001))
     u.mem_write(parent+0x28,bytes([0x71]));u.mem_write(parent+0x1f8,pack(0x87654321))
    p=bytearray(0xa0);struct.pack_into('<I',p,0x10,0x12345678);p[0x3c:0x48]=position;p[0x48:0x6c]=matrix
    struct.pack_into('<f',p,0x84,radius);struct.pack_into('<I',p,0x94,0xffffffff);u.mem_write(params,bytes(p))
    calls.clear();u.mem_write(stack,pack(stop,kind,7,0x43210001,params,flags,0));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x486da0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==base
    assert calls==[0x487100,0x4ffa80,0x49ec90]
    expected=bytearray(before)
    final_flags=flags|0x6000000|(0x8000 if flags&0x4000 else 0)|(0x400000 if kind not in (5,6,8,9,10) else 0)
    fields={0:0,0x20:100,0x24:kind,0x2c:0x10000,0x30:0x43210001,0x34:0x42c80000,0x38:0,0x7c:final_flags,0x80:0,0x84:0xffffffff,0x1f8:0x87654321 if valid_parent else 1,0x1fc:0x12345678,0x200:0xffffffff,0x204:0xffffffff,0x26c:0xffffffff,0x274:0xffffffff,0x278:0}
    for offset,value in fields.items():expected[offset:offset+4]=pack(value)
    expected[0x28]=0x71 if valid_parent else 0;expected[0x270]=255
    for offset in (4,0x3c,0x6c,0x238):expected[offset:offset+12]=position
    for offset in (0x48,0x244):expected[offset:offset+36]=matrix
    expected[0x78:0x7c]=struct.pack('<f',radius if radius>0 else 1.0)
    actual=bytes(u.mem_read(base,0x1494))
    assert actual==expected,(kind,flags,valid_parent,radius,[hex(i) for i,(a,e) in enumerate(zip(actual,expected)) if a!=e][:20])
    assert read(0x7394cc)==base and read(0x59f7e4)==99
    if radius<0:p[0x84:0x88]=struct.pack('<f',1.0)
    if flags&0x10000:struct.pack_into('<I',p,0x94,0xffffffdf)
    assert bytes(u.mem_read(params,len(p)))==p
    cases+=1
report=dict(result='PASS',cases=cases,scope='Complete generic 486da0 no-model/no-world path with allocator, name assignment and physics initialization intercepted. Real handle pool, parent lookup, vector/matrix copies and spatial assignment execute. Full object/parameter byte comparisons. No allocation, name storage, physics, model loading or gameplay integration.')
(root/'artifacts/object-factory-fields-verification.json').write_text(json.dumps(report,indent=2));print(report)

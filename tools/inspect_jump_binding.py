"""Execute default jump binding setup and original keyboard action queries."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
binary=root/'Installed_Game/RF.exe';sha=hashlib.sha256(binary.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(binary));im=p.get_memory_mapped_image();p.close();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);controls=base;edge=base+0x2000;lock=base+0xc000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
menu=reserved=text_entry=0
# Native synchronization is replaced only by process-local RET 4 stubs.
u.mem_write(lock,b'\xc2\x04\x00');u.mem_write(0x589114,w(lock));u.mem_write(0x589110,w(lock))
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);pop=4
 if address==0x4ffa80:value=cpu.reg_read(UC_X86_REG_ECX);pop=8
 elif address==0x444ac0:value=menu
 elif address==0x43d470:assert read(sp+4)==0x39;value=reserved
 else:value=text_entry
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+pop)
for address in (0x4ffa80,0x444ac0,0x43d470,0x50b520):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
# Execute the actual setup through its fourth registration (action 3).
u.mem_write(stack,w(stop,controls));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x43d060,0x43d0c5,count=10000)
assert u.reg_read(UC_X86_REG_EIP)==0x43d0c5 and read(controls+0xe0c)==4
record=bytes(u.mem_read(controls+0xc+3*28,28));kind=struct.unpack_from('<I',record,8)[0];keys=struct.unpack_from('<3h',record,20)
assert kind==0 and keys==(0x39,-1,-1)
u.mem_write(0x1886a1c,b'\x01');counter=0x18860e8+0x39*4;held=0x18868f4+0x39
results=[]
def query():
 u.mem_write(edge,b'\xa5');u.mem_write(stack,w(stop,controls,3,edge));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x43d4f0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 assert bytes(u.mem_read(controls+0xc+3*28,28))==record
 return bool(u.reg_read(UC_X86_REG_EAX)&255),u.mem_read(edge,1)[0],read(counter)
for menu,reserved,text_entry,count,down in itertools.product((0,1),(0,1),(0,1),(0,1,3),(0,1)):
 u.mem_write(counter,w(count));u.mem_write(held,bytes([down]));active,pressed,remaining=query()
 allowed=not(menu and reserved) and not text_entry
 assert active==bool(allowed and count) and pressed==int(active)
 assert remaining==(count if text_entry and not(menu and reserved) else 0)
 results.append(dict(menu=menu,reserved=reserved,text_entry=text_entry,count=count,held=down,active=active,pressed=pressed,remaining=remaining))
menu=reserved=text_entry=0;sequence=[]
for name,count,down in [('press',1,1),('hold',None,1),('hold_again',None,1),('release',None,0),('repress',1,1)]:
 if count is not None:u.mem_write(counter,w(count))
 u.mem_write(held,bytes([down]));result=query();sequence.append(dict(phase=name,active=result[0],pressed=result[1],remaining=result[2]))
assert [x['active'] for x in sequence]==[True,False,False,False,True]
report=dict(result='PASS',original_sha256=sha,cases=len(results),binding_type=kind,keys=keys,sequence=sequence,results=results,scope='Actual default initializer slice, action query and press-counter consumption. String assignment and menu/reserved/text-entry queries supplied; lock calls process-local stubs. No host input, rebinding, mouse binding or live gameplay.')
(root/'artifacts/jump-binding.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'jump binding cases; press/hold/release sequence', [x['active'] for x in sequence])

"""Verify original post-update jump/support dispatch, after 41e4b0."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_EAX
binary=root/'Installed_Game/RF.exe';sha=hashlib.sha256(binary.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(binary));im=p.get_memory_mapped_image();p.close();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);descriptor=base+0x3000;stack=base+0xe000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
calls=[]
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);assert read(sp+4)==base
 if address==0x429990:value=kind_one
 else:calls.append('fall' if address==0x4281a0 else 'support');value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in (0x429990,0x4281a0,0x4a0840):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
results=[]
for mode,jump,kind_one,attachment,parent,moved,supported,object_bit in itertools.product(range(16),(0,1),(0,1),(-1,4),(-1,4),(0,1),(0,1),(0,1)):
 seed=bytearray(0x1500)
 for offset,value in ((0x858,descriptor),(0x810,jump*2),(0x1380,attachment),(0x200,parent),(0x1a8,supported*0x400000),(0x7c,object_bit*8)):struct.pack_into('<I',seed,offset,value&0xffffffff)
 u.mem_write(base,bytes(seed));u.mem_write(descriptor,w(1,mode));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,moved);u.reg_write(UC_X86_REG_EAX,jump*2)
 # 487f73 removes the preceding 41e4b0 argument before making this decision.
 u.mem_write(stack-4,w(base));u.reg_write(UC_X86_REG_ESP,stack-4);calls.clear();u.emu_start(0x487f73,0x487fc9,count=1000)
 assert u.reg_read(UC_X86_REG_EIP)==0x487fc9 and u.reg_read(UC_X86_REG_ESP)==stack
 airborne=mode in (3,8) or (kind_one and attachment==-1)
 route='fall' if jump else 'support' if airborne or (mode==1 and parent==-1 and (moved or supported or object_bit)) else 'none'
 assert calls==([] if route=='none' else [route]) and bytes(u.mem_read(base,len(seed)))==seed
 results.append(dict(mode=mode,jump=jump,kind_one=kind_one,attachment=attachment,parent=parent,moved=moved,supported=supported,object_bit=object_bit,route=route))
report=dict(result='PASS',cases=len(results),original_sha256=sha,scope='Prepared 487f73..487fc9 after actor update. Actual 42a020/4895d0; kind-one predicate and fall/support boundaries supplied. Does not prove flag lifetime, support geometry or update order in the port.',results=results)
(root/'artifacts/jump-support-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'original post-update support dispatch cases')

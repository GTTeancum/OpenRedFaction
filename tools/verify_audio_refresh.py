"""Execute original positional refresh through intercepted final device setters."""
import hashlib,json,struct,sys,itertools
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;volume_set=stop+0x100;pan_set=stop+0x200;m.mem_map(base,65536)
u=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
trace=[]
def hook(machine,address,size,user):
 sp=machine.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',machine.mem_read(sp,4))[0]
 if address==0x522f30:machine.reg_write(UC_X86_REG_EAX,0);pop=4
 else:
  this,value=struct.unpack('<2I',machine.mem_read(sp+4,8));assert this==base+256
  trace.append([address==pan_set,value]);machine.reg_write(UC_X86_REG_EAX,0);pop=12
 machine.reg_write(UC_X86_REG_ESP,sp+pop);machine.reg_write(UC_X86_REG_EIP,ret)
for addr in [0x522f30,volume_set,pan_set]:m.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
def call(addr,args=b''):
 m.mem_write(stack,u(stop)+args);m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_FPCW,0x27f);m.emu_start(addr,stop,count=100000);assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
call(0x521680)
m.mem_write(0x1cfc5d0,u(1,1));m.mem_write(0x1754160,f(0,0,0));m.mem_write(0x1753c28,f(1,0,0))
m.mem_write(base+256,u(base+512));m.mem_write(base+512+0x3c,u(volume_set,pan_set));m.mem_write(0x1ad7520,u(base+256))
rows=[]
for distance,requested,group,sample_volume in itertools.product([-40.,-10.,0.,10.,40.],[.25,.5,1.,2.],[.25,1.],[.1,.9]):
 m.mem_write(base,f(distance,0,0));m.mem_write(0x1cd3ba8+0x20,f(sample_volume,5,32,1));m.mem_write(0x1753c18,f(group))
 # Direct original spatial result supplies expected input to the original table.
 call(0x505740,u(0,base,base+32,base+36)+f(requested));pan,gain=struct.unpack('<2f',m.mem_read(base+32,8))
 gain=struct.unpack('<f',f(gain*group))[0];want_volume=call(0x522420,f(min(1,max(0,gain))));want_pan=int(min(1,max(-1,pan))*1000)&0xffffffff
 record=bytearray(44);record[:16]=u(7,0,0,2);record[40]=1;m.mem_write(0x1753c38,bytes(record));trace.clear()
 call(0x5058c0,u(0x200,base,0x173c378)+f(requested))
 assert trace==[[False,want_volume],[True,want_pan]],(distance,requested,group,sample_volume,trace,want_volume,want_pan)
 rows.append(trace.copy())
report=dict(result='PASS',cases=len(rows),original_sha256=digest,scope='Original5058c0 ->544390/544450 ->522d30/522d80 and spatial/table/ftol code intact; only hardware handle resolution and final device setters intercepted. Sample default volume varied independently. No native scene integration.')
(root/'artifacts/audio-refresh-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

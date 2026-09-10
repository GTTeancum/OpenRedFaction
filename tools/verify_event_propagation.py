"""Compare original ordered link walking with PC/NXDK, including list mutation."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
def words(*v):return struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
original=root/'Installed_Game/RF.exe';sha=hashlib.sha256(original.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe')
base=0x30000000;data=base+0x1000;alternate=data+0x100;stack=base+0xe000;stop=base+0xf000;callback=stop+16
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
calls=trace=mutation=0
def hook(cpu,address,size,unused):
 global calls,trace
 if address not in (0x4b65c0,0x4b6640,callback):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(cpu,sp);native=address==callback
 if native:values=struct.unpack('<5I',cpu.mem_read(sp+8,20))
 else:
  target,source,actor=struct.unpack('<3I',cpu.mem_read(sp+4,12))
  on=int(address==0x4b65c0);suppress=0 if on else read(cpu,sp+16)
  values=(target,source,actor,on,suppress)
 for v in values:trace=((trace^v)*16777619)&0xffffffff
 if not calls:
  owner=base if native else base+0x29c
  if mutation==1:cpu.mem_write(owner,words(0))
  if mutation==2:cpu.mem_write(owner,words(8))
  if mutation==3:cpu.mem_write(owner+(4 if native else 8),words(alternate))
  if mutation==4:cpu.mem_write(data+4,words(-1))
 calls+=1;cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
entry=int(re.search(r'_rf_event_links_propagate\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=bytearray();expected=bytearray();cases=0
for count,source,actor,mode,change in itertools.product((0,1,2,4,8),(0,0xffffffff),(7,0x12345678),(0,1,2,255,256,257,0xffffffff),range(5)):
 mutation=change;outputs=[]
 for cpu,native in ((u,False),(x,True)):
  cpu.mem_write(base,bytes(0x400));cpu.mem_write(data,words(*range(100,108)));cpu.mem_write(alternate,words(*range(200,208)))
  cpu.mem_write(base if native else base+0x29c,words(count,data) if native else words(count,8,data))
  args=words(stop,base,source,actor,mode,callback,0) if native else words(stop,source,actor,mode)
  cpu.mem_write(stack,args);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,base)
  calls=0;trace=2166136261;cpu.emu_start(entry if native else 0x4b8b00,stop,count=10000)
  assert cpu.reg_read(UC_X86_REG_EIP)==stop
  assert cpu.reg_read(UC_X86_REG_ESP)==stack+(4 if native else 16)
  status=cpu.reg_read(UC_X86_REG_EAX) if native else 0
  outputs.append(words(status,calls,trace,read(cpu,base if native else base+0x29c)))
 assert outputs[0]==outputs[1],(count,source,actor,mode,change,outputs)
 commands.extend(words(count,source,actor,mode,change));expected.extend(outputs[0]);cases+=1
probe=root/'build/pc/Release/rf_event_probe.exe'
assert subprocess.check_output([str(probe),'--propagation'],input=commands)==expected
report=dict(result='PASS',cases=cases,original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original 4b8b00 and array access execute unchanged. Generic on/off dispatch intercepted. PC/NXDK exact callback order/arguments and final count including shrink, growth, replacement and next-handle mutation. Target dispatch, recursion and scene wiring excluded.')
(root/'artifacts/event-propagation-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

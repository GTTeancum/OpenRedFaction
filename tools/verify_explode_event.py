"""Verify original Explode request arguments/order without executing effects."""
import hashlib,itertools,json,struct,sys,re,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,65536);stack=base+0xe000;stop=base+0xf000
def write(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def block(a,n):return list(struct.unpack('<'+'I'*n,u.mem_read(a,n*4)))
trace=[]
def hook(cpu,address,size,unused):
 if address not in (0x467020,0x436490):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(sp);args=block(sp+4,7)
 if address==0x467020:
  assert args[3]==base+0x40
  trace.append(dict(call='467020',args=args[:3]+[block(args[3],3),block(args[4],3)]+args[5:]))
 else:
  assert args[3]==base+0x40
  trace.append(dict(call='436490',args=args[:3]+[block(args[3],3)]+args[4:]))
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);results=[];commands=bytearray();expected=bytearray()
position=[0x42e00000,0xc1200000,0x3f800000]
for flag,room,effect,scale,secondary,on in itertools.product((0,1,2,255,256,257),(0,0x34567890),(-1,0,7),(0,0x3f400000,0x40000000),(0,0x3f800000),(0,1)):
 u.mem_write(base,bytes(0x400));write(base,0x58998c,room);write(base+0x40,*position)
 write(base+0x290,10);write(base+0x2b8,flag);write(base+0x2c4,effect,secondary,scale)
 before=bytes(u.mem_read(base,0x2d0));trace.clear();write(stack,stop)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
 u.emu_start(0x4bae20 if on else 0x4b9f80,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 wanted=[]
 if on:
  if (flag&255)==1 and room:wanted.append(dict(call='467020',args=[scale,0xffffffff,room,position,[0x3f800000,0,0],0,1]))
  wanted.append(dict(call='436490',args=[effect&0xffffffff,room,0,position,scale,secondary,0]))
 assert trace==wanted,(flag,room,effect,scale,secondary,on,trace,wanted)
 assert bytes(u.mem_read(base,0x2d0))==before
 command=[flag,room,*position,effect&0xffffffff,scale,secondary,on]
 commands.extend(struct.pack('<9I',*command));digest=2166136261
 for call in trace:
  a=call['args']
  request=[1,a[1],a[2],*a[3],*a[4],a[0],0] if call['call']=='467020' else [0,a[0],a[1],*a[3],0,0,0,a[4],a[5]]
  for word in request:digest=((digest^word)*16777619)&0xffffffff
 expected.extend(struct.pack('<3I',0,len(trace),digest))
 results.append(dict(flag=flag,room=room,effect=effect,scale=scale,secondary=secondary,on=on,trace=list(trace)))
probe=root/'build/pc/Release/rf_event_probe.exe'
assert subprocess.check_output([str(probe),'--explode'],input=commands)==expected
p=pefile.PE(str(root/'build/xbox/main.exe'));image=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(image)+4095)//4096*4096);x.mem_write(origin,image);x.mem_map(base,65536)
entry=int(re.search(r'_rf_event_explode_action\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
callback=stop+16;count=digest=0
def compiled_hook(cpu,address,size,unused):
 global count,digest
 if address!=callback:return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,context,request=struct.unpack('<3I',cpu.mem_read(sp,12))
 assert context==0
 for word in struct.unpack('<11I',cpu.mem_read(request,44)):digest=((digest^word)*16777619)&0xffffffff
 count+=1;cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,compiled_hook)
for i in range(len(results)):
 command=commands[i*36:i*36+36];x.mem_write(base,bytes(command[:32]));action=struct.unpack_from('<I',command,32)[0]
 x.mem_write(stack,struct.pack('<5I',stop,base,action,callback,0));x.reg_write(UC_X86_REG_ESP,stack)
 count=0;digest=2166136261;x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
 assert bytes(x.mem_read(base,32))==command[:32]
 assert struct.pack('<3I',x.reg_read(UC_X86_REG_EAX),count,digest)==expected[i*12:i*12+12],i
report=dict(result='PASS',cases=len(results),original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original virtual Explode actions with consumers intercepted; normalized request arguments/order match PC and compiled NXDK emitter. No geometry, rendering, audio, damage, lookup, callback mutation or scene integration claimed.',results=results)
(root/'artifacts/explode-event-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report['result'],report['cases'])

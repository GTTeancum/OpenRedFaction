"""Verify original Explode request arguments/order without executing effects."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
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
u.hook_add(UC_HOOK_CODE,hook);results=[]
position=[0x42e00000,0xc1200000,0x3f800000]
for flag,room,effect,scale,secondary,on in itertools.product((0,1,2,255),(0,0x34567890),(-1,0,7),(0,0x3f400000,0x40000000),(0,0x3f800000),(0,1)):
 u.mem_write(base,bytes(0x400));write(base,0x58998c,room);write(base+0x40,*position)
 write(base+0x290,10);write(base+0x2b8,flag);write(base+0x2c4,effect,secondary,scale)
 before=bytes(u.mem_read(base,0x2d0));trace.clear();write(stack,stop)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
 u.emu_start(0x4bae20 if on else 0x4b9f80,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 wanted=[]
 if on:
  if flag==1 and room:wanted.append(dict(call='467020',args=[scale,0xffffffff,room,position,[0x3f800000,0,0],0,1]))
  wanted.append(dict(call='436490',args=[effect&0xffffffff,room,0,position,scale,secondary,0]))
 assert trace==wanted,(flag,room,effect,scale,secondary,on,trace,wanted)
 assert bytes(u.mem_read(base,0x2d0))==before
 results.append(dict(flag=flag,room=room,effect=effect,scale=scale,secondary=secondary,on=on,trace=list(trace)))
report=dict(result='PASS',cases=len(results),original_sha256=sha,scope='Complete original virtual Explode on 4bae20/off 4b9f80 and vector/room helpers. Geometry/effect routines 467020/436490 intercepted. Exact argument words, order and unchanged event storage; no geometry, rendering, audio, damage, lookup or shared-code equivalence claimed.',results=results)
(root/'artifacts/explode-event-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report['result'],report['cases'])

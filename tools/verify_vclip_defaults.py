"""Execute original vclip fixed-field loading with parser/resource seams supplied."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b);u.mem_map(0,4096)
base=0x30000000;u.mem_map(base,65536);stack=base+0xe000;stop=base+0xf000;float_return=stop+16
def write(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def string(a):
 data=bytearray()
 while True:
  ch=u.mem_read(a,1)[0];a+=1
  if not ch:return data.decode('ascii')
  data.append(ch)
fields=['$Flags:','$damage:','$VBM Filename:','$VBM Glow:','$Explosion Name:','$VFX Filename:','$VFX Radius:','$Foley Sound:']
mask=0;label=''
u.mem_write(base+0x1000,b'fixture\0');write(base+0x1100,0x40600000)
u.mem_write(float_return,b'\xd9\x05'+struct.pack('<I',base+0x1100)+b'\xc3')
def hook(cpu,address,size,unused):
 global label
 pop={0x5126a0:4,0x5125c0:4,0x512bb0:12,0x513020:8,0x512920:0,0x5126f0:0,0x512c20:16,0x4ffa80:4,0x434cb0:0,0x4ff470:0}
 if address not in pop:return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(sp);value=0
 if address in (0x5126a0,0x5125c0):
  label=string(read(sp+4))
  value=int(label in fields and bool(mask&(1<<fields.index(label))))
 elif address==0x512bb0:write(read(sp+4),7,base+0x1000)
 elif address==0x512c20:u.mem_write(read(sp+4),b'fixture\0')
 elif address==0x513020:value=9
 elif address==0x5126f0:value=1
 elif address==0x434cb0:value=17
 elif address==0x4ffa80:write(cpu.reg_read(UC_X86_REG_ECX),0,0)
 elif address==0x512920:
  cpu.reg_write(UC_X86_REG_EIP,float_return);return
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop[address]);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);results=[]
for slot in (0,63):
 for mask in range(256):
  target=0x858cb8+slot*224;u.mem_write(target,bytes(224));write(0x8568ac,slot);write(0,0xffffffff)
  write(stack,stop,base+0x2000);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c1460,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
  assert read(0x8568ac)==slot+1
  expected={8:0x40600000 if mask&2 else 0,0x30:9 if mask&1 else 0,0x2c:17 if mask&128 else 0xffffffff,0x34:0,0xbc:0x40600000 if mask&64 else 0x41a00000}
  for offset,value in expected.items():assert read(target+offset)==value,(slot,mask,offset,read(target+offset),value)
  assert u.mem_read(target+0x20,1)[0]==int(bool(mask&4 and mask&8))
  assert u.mem_read(target+0xc0,1)[0]==(ord('f') if mask&16 else 0)
  results.append(dict(slot=slot,presence_mask=mask,words=expected))
report=dict(result='PASS',cases=len(results),original_sha256=sha,scope='Original 4c1460 control flow and fixed-field writes execute. Parser reads, string ownership and Foley resolution supplied; zeroed definition storage. Embedded particle block omitted. No actual table parsing, shared loader, allocation or asset resolution equivalence.',results=results)
(root/'artifacts/vclip-defaults-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report['result'],report['cases'])

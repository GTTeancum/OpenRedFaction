"""Original auto sweep bookkeeping versus shared PC and compiled NXDK.
Link dispatch is intercepted; callback observes old state without mutation.
"""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
offsets=(0x2b0,0x2a0,0x298,0x29c,0x2a8,0x2c)
def state():return w(*[read(u,base+o) for o in offsets])
calls=trace=0
callback=base+0x4000
def hook(cpu,address,size,data):
 global calls,trace
 if address not in (0x4c0320,callback):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(cpu,sp)
 if address==0x4c0320:
  assert read(cpu,sp+4)==base and read(cpu,sp+8)==0xffffffff and read(cpu,sp+12)==0
  values=struct.unpack('<6I',state())
 else:
  assert read(cpu,sp+8)==base and read(cpu,sp+12)==0xffffffff and read(cpu,sp+16)==0
  values=struct.unpack('<6I',cpu.mem_read(base,24))
 trace=0xffffffff
 for value in values:trace^=value
 calls+=1;cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
commands=bytearray();expected=bytearray()
for flags in (0,8,24,72,88,0xffffffff):
 for count in (0,5,0xffffffff):
  for cooldown in (-1,0,1,30000,1072800000):
   for now in (0,12345,1072799999,1072800000):
    u.mem_write(base,bytes(0x400));u.mem_write(0x856844,w(0));u.mem_write(0x8567ac,w(base))
    u.mem_write(base+0x28c,w(0x856520));u.mem_write(base+0x2a4,w(-1));u.mem_write(base+0x2ac,w(-1));u.mem_write(0x64ecb9,b'\0\0')
    before=w(flags,count,777,cooldown,0x40500000,0x12340005)
    for i,o in enumerate(offsets):u.mem_write(base+o,before[i*4:i*4+4])
    u.mem_write(0x5a3ed8,w(now));u.mem_write(0x6460f0,w(0x41400000))
    commands.extend(before+w(now,0x41400000,1));calls=trace=0
    u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c01b0,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
    expected.extend(state()+w(0,calls,trace))
probe=root/'build/pc/Release/rf_event_probe.exe'
assert subprocess.check_output([str(probe),'--auto-trigger'],input=commands)==expected
entry=int(re.search(r'_rf_auto_trigger_fire\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i in range(len(commands)//36):
 before=commands[i*36:i*36+24];now,clock,eligible=struct.unpack_from('<3I',commands,i*36+24)
 x.mem_write(base,bytes(before));x.mem_write(stack,w(stop,base,now,clock,eligible,callback,0));x.reg_write(UC_X86_REG_ESP,stack);calls=trace=0
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
 assert bytes(x.mem_read(base,24))+w(x.reg_read(UC_X86_REG_EAX),calls,trace)==expected[i*36:i*36+36],i
report=dict(result='PASS',cases=len(commands)//36,original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original 4c01b0 sweep and 4c0220 bookkeeping/timer; link dispatch intercepted with old-state observation. PC/NXDK exact state and callback arguments match. Global/script eligibility supplied; callback mutation, registry, links and live campaign wiring excluded.')
(root/'artifacts/auto-trigger-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

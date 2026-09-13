"""Verify angular dispatch precedence against original49f3c0."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')

from unicorn import UC_HOOK_CODE
entry=int(re.search(r'\s_rf_movement_angular_route\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
terminals={0x49f446:0,0x484650:1,0x49e180:2,0x49e050:3,0x49de50:4,0x49f646:5}
def stop_at_branch(cpu,address,size,data):
 if address in terminals:cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,stop_at_branch)
commands=[];expected=[];counts=[0]*6
for combination in range(16):
 flags=sum(bit for i,bit in enumerate((0x8000,0x4000,0x80,0x1000000)) if combination&(1<<i))
 for mode in (0,1,11,15,0xffffffff):
  for control in (0,1,255,256,257):
   for primary in (0,1):
    command=w(flags,mode,control,primary);commands.append(command)
    u.mem_write(B,bytes(0x3000));u.mem_write(B+0x1a8,w(flags));u.mem_write(B+0x858,w(B+0x2000));u.mem_write(B+0x2004,w(mode));u.mem_write(B+0x720,bytes([control&255]));u.mem_write(0x5cb054,w(B if primary else B+0x1000));u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK)
    u.emu_start(0x49f3c0,STOP,count=10000);pc=u.reg_read(UC_X86_REG_EIP);assert pc in terminals
    value=terminals[pc];expected.append(w(value));counts[value]+=1
    x.mem_write(STACK,w(STOP,flags,mode,control,primary));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==value
actual=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe'),'--angular-route'],input=b''.join(commands));assert actual==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),route_counts=counts,scope='Original49f3c0 executes to first angular dispatch boundary; no special callees or numeric work executed. Exhaustive four relevant flag combinations, five modes, five control words and primary/nonprimary identity. No live physics scheduling claim.')
(root/'artifacts/angular-route.json').write_text(json.dumps(report,indent=2));print(report)

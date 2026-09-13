"""Verify ordinary angular velocity preparation against original instruction paths."""
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

from unicorn.x86_const import UC_X86_REG_ESI
entry=int(re.search(r'\s_rf_angular_velocity_step\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x49f485);commands=[];expected=[]
for n in range(2048):
 driven=[0,1,2,255,256][n%5];flags=0x1000000 if n%13==0 else 0
 values=[rng.uniform(-10,10) for _ in range(9)]+[rng.uniform(.1,20),rng.uniform(.1,100),rng.uniform(.1,100),rng.uniform(0,2)]
 if n<20:values[-1]=0
 command=f(*values)+w(flags,driven);commands.append(command)
 u.mem_write(B,bytes(0x3000));u.mem_write(STACK,bytes(0x400));u.mem_write(B+0x294,w(B+0x2000));u.mem_write(B+0x2060,command[36:44]);u.mem_write(B+0x98,command[44:48]);u.mem_write(B+0x1b0,command[48:52]);u.mem_write(B+0x708,command[24:36]);u.mem_write(B+0x150,command[:12]);u.mem_write(B+0x174,command[12:24]);u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_FPCW,0x37f)
 if not flags:
  u.emu_start(0x49f485 if driven&255 else 0x49f527,0x49f566,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x49f566
 result=w(0)+bytes(u.mem_read(B+0x150,12))+bytes(u.mem_read(B+0x174,12));expected.append(result)
 x.mem_write(B,command);args=struct.unpack('<4I',command[36:52]);x.mem_write(STACK,w(STOP,B,B+24,*args,flags,driven));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,24));assert actual==result,('NXDK',n,actual.hex(),result.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--angular-velocity'],input=b''.join(commands))
for n,result in enumerate(expected):assert actual[n*28:(n+1)*28]==result,('PC',n,actual[n*28:(n+1)*28].hex(),result.hex())

guards=[]
for at,value in [(0,float('nan')),(12,float('inf')),(24,float('nan')),(36,0),(40,0),(44,0),(48,-1)]:
 command=bytearray(commands[1]);command[at:at+4]=f(value);command[52:60]=w(0,1);guards.append(bytes(command))
 x.mem_write(B,bytes(command));x.mem_write(STACK,w(STOP,B,B+24,*struct.unpack('<4I',command[36:52]),0,1));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B,24))==command[:24]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--angular-velocity'],input=b''.join(guards))
assert actual==b''.join(w(0xfffffffc)+command[:24] for command in guards)
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),pc_nxdk_error_guards=len(guards),scope='Original49f485 or49f527 through49f565, actual x87 exponential and vector callees; drive predicate resolved by fixture. Flag1000000 cases compare unchanged state. No special-mode dispatch, live NPC scheduling or prediction claim.')
(root/'artifacts/angular-velocity.json').write_text(json.dumps(report,indent=2));print(report)

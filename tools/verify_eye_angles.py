"""Compare full unhooked49cf40 eye-angle update with shared PC/NXDK."""
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
entry=int(re.search(r'\s_rf_eye_angles_step\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x49cf40);commands=[];expected=[];mapping=[(0,0x870,48),(48,0x1434,24)]
for n in range(2048):
 flags=[0,0x200,0x1000,0x400000,0x401200,0x80000000][n%6]
 dt=[0,1/60,1/30,.1,1,10][n%6] if n<64 else rng.uniform(0,2)
 values=[rng.uniform(-20,20) for _ in range(18)]
 if n<64:values[3:6]=[6.2831854820251465,-6.2831854820251465,0];values[6:9]=[0,0,0]
 if n%3:values[12:15]=[-100]*3;values[15:18]=[100]*3
 command=f(*values)+w(flags)+f(dt);commands.append(command)
 u.mem_write(B,bytes(0x3000));u.mem_write(B+0x294,w(B+0x2000));u.mem_write(B+0x2724,w(flags));u.mem_write(0x5a4014,f(dt))
 for at,offset,size in mapping:u.mem_write(B+offset,command[at:at+size])
 u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49cf40,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 result=b''.join(bytes(u.mem_read(B+offset,size)) for at,offset,size in mapping);expected.append(w(0)+result)
 x.mem_write(B,command);x.mem_write(STACK,w(STOP,B,flags,struct.unpack('<I',f(dt))[0]));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,72))
 assert actual==expected[-1],('NXDK',n,actual.hex(),expected[-1].hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--eye-angles'],input=b''.join(commands))
assert actual==b''.join(expected),'PC differs from original'
guards=[]
for at,value,flags in [(76,float('nan'),0x200),(76,-1,0x200),(0,float('inf'),0x200),(12,float('nan'),0),(24,float('inf'),0),(48,float('nan'),0)]:
 command=bytearray(f(*([0]*12+[-100]*3+[100]*3))+w(flags)+f(.1));command[at:at+4]=f(value);guards.append(bytes(command))
 x.mem_write(B,bytes(command));x.mem_write(STACK,w(STOP,B,flags,struct.unpack_from('<I',command,76)[0]));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B,72))==command[:72]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--eye-angles'],input=b''.join(guards))
assert actual==b''.join(w(0xfffffffc)+command[:72] for command in guards)
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),pc_nxdk_error_guards=len(guards),scope='Full49cf40 and actual42d7b0 predicate/vector/clamp/x87 exponential; no substituted original callees. All72 retained bytes compared. Eye matrix and live NPC integration remain separate.')
(root/'artifacts/eye-angles.json').write_text(json.dumps(report,indent=2));print(report)

"""Verify49fe7a eye matrix composition using full original rotation callees."""
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

from unicorn.x86_const import UC_X86_REG_ECX
entry=int(re.search(r'\s_rf_eye_physics_orientation\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x49fe7a);commands=[];expected=[]
for n in range(2048):
 body=[rng.uniform(-2,2) for _ in range(9)]
 if n<32:body=[1,0,0,0,1,0,0,0,1]
 if 32<=n<64:body=[(0,.00009,.0001,.00011)[n%4],(-1,1)[n%2],0,1,0,0,0,0,1]
 values=[rng.uniform(-1,1) for _ in range(18)]
 if n<16:values=[0]*18
 command=f(*body,*values);commands.append(command)
 u.mem_write(B,command[:36])
 for target,index in [(0x4fd240,0),(0x4fd310,2)]:
  angle=struct.unpack('<f',f(struct.unpack_from('<f',command,36+36+index*4)[0]+struct.unpack_from('<f',command,36+12+index*4)[0]))[0]
  u.mem_write(STACK,w(STOP)+f(angle));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,B);u.reg_write(UC_X86_REG_FPCW,0x37f)
  u.emu_start(target,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 result=w(0)+bytes(u.mem_read(B,36));expected.append(result)
 x.mem_write(B,command);x.mem_write(STACK,w(STOP,B,B+36,B));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,36));assert actual==result,('NXDK',n,actual.hex(),result.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--physics-orientation'],input=b''.join(commands))
assert actual==b''.join(expected),'PC differs'
guards=[]
for at,value in [(0,float('nan')),(36+12,float('inf'))]:
 command=bytearray(commands[0]);command[at:at+4]=f(value);guards.append(bytes(command))
command=bytearray(commands[0]);command[:12]=bytes(12);guards.append(bytes(command))
for command in guards:
 x.mem_write(B,command);x.mem_write(STACK,w(STOP,B,B+36,B));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B,36))==command[:36]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--physics-orientation'],input=b''.join(guards))
assert actual==b''.join(w(0xfffffffc)+command[:36] for command in guards)
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),pc_nxdk_error_guards=len(guards),scope='Full unhooked4fd240 then4fd310, including axis normalization/alignment, angle matrices, transforms and crosses; all9 output floats compared with PC/NXDK. Aliased output, vertical threshold, zero angles and nonunit input bases. This covers49fe7a matrix composition, not full49fe40 or live NPC integration.')
(root/'artifacts/eye-physics-orientation.json').write_text(json.dumps(report,indent=2));print(report)

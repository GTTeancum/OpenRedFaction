"""Compare ordinary49f566..49f634 angular prediction with PC/NXDK."""
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
entry=int(re.search(r'\s_rf_angular_predict\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x49f566);commands=[];expected=[]
for n in range(2048):
 angles=[rng.uniform(-1.5,1.5),rng.uniform(-6,6),rng.uniform(-6,6)];velocity=[rng.uniform(-20,20) for _ in range(3)];dt=[0,1/60,1/30,.1][n%4]
 refs=[(n//4**i)%4 for i in range(3)] if n<64 else [rng.choice([0,1,2,3,4,0xffffffff]) for _ in range(3)]
 command=f(*angles,*velocity,dt)+w(*refs);commands.append(command)
 u.mem_write(B,bytes(0x3000));u.mem_write(STACK,bytes(0x400));u.mem_write(B+0x858,w(B+0x2000));u.mem_write(B+0x2014,w(*refs))
 u.mem_write(B+0x864,command[:12]);u.mem_write(B+0x150,command[12:24]);u.mem_write(B+0x1b0,command[24:28]);u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49f566,0x49f646,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x49f646
 result=w(0)+bytes(u.mem_read(B+0x870,12))+bytes(u.mem_read(STACK+0x10,12))+bytes(u.mem_read(B+0xfc,72))+bytes(u.mem_read(B+0x888,12));expected.append(result)
 x.mem_write(B,command);x.mem_write(B+0x4000,b'\xa5'*108);x.mem_write(STACK,w(STOP,B,B+12,struct.unpack_from('<I',command,24)[0],B+28,B+0x4000));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x4000,108));assert actual==result,('NXDK',n,actual.hex(),result.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--angular-predict'],input=b''.join(commands))
assert len(actual)==112*len(expected),(len(actual),len(expected))
max_error=0;different=0
for n,result in enumerate(expected):
 got=actual[n*112:(n+1)*112]
 assert got[:28]==result[:28] and got[100:]==result[100:],('PC angular fields',n)
 for a,b in zip(struct.unpack('<18f',got[28:100]),struct.unpack('<18f',result[28:100])):
  max_error=max(max_error,abs(a-b));assert abs(a-b)<=1e-15,('PC matrix',n,a,b)
 different+=got!=result
print('PC matrix differing cases/max absolute error:',different,max_error)
guards=[]
for at,value in [(0,float('nan')),(12,float('inf')),(24,-1),(24,float('nan'))]:
 command=bytearray(commands[0]);command[at:at+4]=f(value);guards.append(bytes(command))
 x.mem_write(B,bytes(command));x.mem_write(B+0x4000,b'\xa5'*108);x.mem_write(STACK,w(STOP,B,B+12,struct.unpack_from('<I',command,24)[0],B+28,B+0x4000));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B+0x4000,108))==b'\xa5'*108
bad=subprocess.check_output([str(root/'build/pc/Release/rf_eye_probe.exe'),'--angular-predict'],input=b''.join(guards))
assert bad==(w(0xfffffffc)+b'\xa5'*108)*len(guards)
report=dict(result='PASS',original_nxdk_exact_cases=len(commands),pc_cases=len(commands),pc_matrix_differing_cases=different,pc_matrix_max_absolute_error=max_error,pc_matrix_absolute_tolerance=1e-15,pc_nxdk_error_guards=len(guards),scope='Original49f566..49f634 composed stage with unhooked vector/descriptor/angle clamp and4a0d70 matrix callees. NXDK all108 bytes exact; PC deltas/angles exact, matrices checked within stated tolerance for tiny pitch-boundary residual differences. Incoming angular velocity is prepared; preceding forces/damping and later collision/commit remain outside this stage.')
(root/'artifacts/angular-predict.json').write_text(json.dumps(report,indent=2));print(report)

"""Execute complete original433c80 and compiled PC/NXDK body rotation filter."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000;OUT=B+0x5000
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_movement_body_rotation\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x433c80);commands=[];expected=[]
for n in range(2048):
 refs=[(n//(4**i))%4 for i in range(3)] if n<64 else [rng.choice([0,1,2,3,4,0xffffffff,0x80000000]) for _ in range(3)]
 values=[rng.getrandbits(32) for _ in range(3)]
 if n<64:values=[0x80000000,0x7fc12345,0x7f800000]
 command=w(*refs,*values);commands.append(command)
 u.mem_write(B+0x858,w(B+0x2000));u.mem_write(B+0x2014,command[:12]);u.mem_write(B+0x4000,command[12:]);u.mem_write(OUT,b'\xa5'*12)
 u.mem_write(STACK,w(STOP,OUT,B,B+0x4000));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x433c80,STOP,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_EAX)==OUT
 result=bytes(u.mem_read(OUT,12));assert result==w(*(0 if r in (0,1) else v for r,v in zip(refs,values)));expected.append(result)
 for alias in (False,True):
  x.mem_write(B,command);dest=B+12 if alias else OUT;x.mem_write(STACK,w(STOP,B,B+12,dest));x.reg_write(UC_X86_REG_ESP,STACK)
  x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0
  assert bytes(x.mem_read(dest,12))==result,(n,alias)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe'),'--body-rotation'],input=b''.join(commands))
assert actual==b''.join(expected)
for args in [(0,B+12,OUT),(B,0,OUT),(B,B+12,0)]:
 x.mem_write(OUT,b'\xa5'*12);x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=10000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(OUT,12))==b'\xa5'*12
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),nxdk_alias_variants=2,null_guards=3,scope='Full unhooked original433c80, all valid reference combinations and arbitrary reference/input bits; PC in-place and NXDK separate/in-place output. No live physics integration claim.')
(root/'artifacts/body-rotation.json').write_text(json.dumps(report,indent=2));print(report)

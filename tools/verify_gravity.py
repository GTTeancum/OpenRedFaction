"""Original gravity setter and event trampoline versus shared PC/NXDK state."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase;p.close();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(im)+4095)//4096*4096);u.mem_write(origin,im);u.mem_map(0x30000000,65536);return u
binary=root/'Installed_Game/RF.exe';sha=hashlib.sha256(binary.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(binary);x=machine(root/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
rng=random.Random(0x4a0e20);values=[0,0x80000000,0x411ccccd,0xc11ccccd,1,0x80000001,0x7f7fffff,0xff7fffff]
while len(values)<1024:
 v=rng.getrandbits(32)
 if v&0x7f800000!=0x7f800000:values.append(v)
expected=[]
for v in values:
 wanted=w(v,0,v^0x80000000,0)
 for entry in (0x4a0e20,0x4bcc00):
  u.mem_write(0x62f2c8,w(0x40a362be));u.mem_write(0x7c7058,b'\xa5'*12);u.mem_write(base+0x2b8,w(v));u.reg_write(UC_X86_REG_ECX,base)
  u.mem_write(stack,w(stop,v));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(entry,stop,count=1000)
  assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
  assert bytes(u.mem_read(0x5a00dc,4))+bytes(u.mem_read(0x7c7058,12))==wanted
  assert bytes(u.mem_read(0x62f2c8,4))==w(0x40a362be)
 expected.append(w(0)+wanted)
# Nonfinite values are defensive errors in the shared API, not original parity cases.
invalid=[0x7f800000,0xff800000,0x7fc00000];values+=invalid;expected += [w(-4)+b'\xa5'*16 for _ in invalid]
probe=root/'build/pc/Release/rf_physics_probe.exe';assert subprocess.check_output([str(probe),'--gravity'],input=w(*values))==b''.join(expected)
entry=int(re.search(r'_rf_physics_gravity_set\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for v,wanted in zip(values,expected):
 x.mem_write(base,b'\xa5'*16);x.mem_write(stack,w(stop,base,v));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=1000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,16))==wanted
report=dict(result='PASS',finite_cases=1024,original_entry_paths=2,shared_nonfinite_cases=3,original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Actual setter 4a0e20 and trampoline 4bcc00; signed scalar/vector and unchanged jump impulse. Event registration/dispatch and level lifecycle outside the fixture.')
(root/'artifacts/gravity-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

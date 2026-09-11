"""Post-landing dispatch and callback mutation ordering."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe');rng=random.Random(0x41993a)
entry=int(re.search(r'\s_rf_entity_landing_finish\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);callback=base+0xc000
special_xor=stance_xor=0;trace=[];nxtrace=[]
def emit(m,ptr,request,original):
 offsets=(0x810,0x1a8) if original else (0,8)
 a=struct.unpack('<I',m.mem_read(ptr+offsets[0],4))[0];b=struct.unpack('<I',m.mem_read(ptr+offsets[1],4))[0]
 (trace if original else nxtrace).extend([request,a,b])
 if request==3:m.mem_write(ptr+offsets[0],pack(a^special_xor))
 else:m.mem_write(ptr+offsets[1],pack(b^stance_xor))
def oh(m,address,size,data):
 if address==0x419981:emit(m,base,3,True);m.reg_write(UC_X86_REG_EIP,0x4199c5)
 elif address in (0x4280b0,0x428030):
  sp=m.reg_read(UC_X86_REG_ESP);request=0 if address==0x4280b0 else 1+bool(struct.unpack('<I',m.mem_read(sp+8,4))[0]&255)
  emit(m,base,request,True);m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+4)
def xh(m,address,size,data):
 if address==callback:
  sp=m.reg_read(UC_X86_REG_ESP);context,ptr,request=struct.unpack('<3I',m.mem_read(sp+4,12));emit(m,ptr,request,False)
u.hook_add(UC_HOOK_CODE,oh);nx.hook_add(UC_HOOK_CODE,xh);nx.mem_write(callback,b'\xc3')
cases=[];expected=[]
for n in range(1024):
 a=rng.getrandbits(32);c=rng.getrandbits(32);b=rng.getrandbits(32);action=[0,4,4,0xffffffff][n%4]
 special_xor=[0,0x400,0xffffffff][n%3];stance_xor=[0,0x200000,0xffffffff][(n//3)%3]
 raw=pack(a,c,b,action,special_xor,stance_xor);cases.append(raw);trace=[];nxtrace=[]
 u.mem_write(base,bytes(0x5000));u.mem_write(base+0x810,pack(a));u.mem_write(base+0x1a8,pack(b));u.mem_write(base+0x520,pack(action));u.mem_write(base+0x294,pack(base+0x4000));u.mem_write(base+0x4724,pack(c))
 u.mem_write(stack,bytes(128));u.mem_write(stack+0x2c,pack(stop));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x41993a,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+0x30
 state=bytes(u.mem_read(base+0x810,4))+pack(c)+bytes(u.mem_read(base+0x1a8,4))+pack(action)
 want=pack(0)+state+pack(len(trace)//3)+pack(*(trace+[0]*(6-len(trace))));expected.append(want)
 nx.mem_write(base,raw[:16]);nx.mem_write(stack,pack(stop,base,callback,0));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(entry,stop,count=1000)
 assert nx.reg_read(UC_X86_REG_EIP)==stop and nx.reg_read(UC_X86_REG_EAX)==0
 assert bytes(nx.mem_read(base,16))==state and nxtrace==trace,('NXDK',n)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--landing-finish'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(cases),original_sha256=sha,scope='Original41993a post-velocity dispatch with actual40a130 predicate. Special AI branch419981..4199c5 and stance routines supplied at explicit callback boundaries; callback flag mutations verify ordering, branch rereads and post-stance force clearing. PC/NXDK final state and request-time flags exact. Does not execute special AI, clearance, stance changes, landing audio or velocity.')
(root/'artifacts/entity-landing-finish.json').write_text(json.dumps(report,indent=2));print(report)

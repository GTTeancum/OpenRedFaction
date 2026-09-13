"""VFX scale/quaternion/translation stages against original math helpers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;FACE=B+0x2000;UV=B+0x3000;PTR=B+0x4000;CTX=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x5473f0);inputs=[];responses=[]
for case in range(1024):
 state=f(*[rng.uniform(-2,2) for _ in range(24)]);poses=f(*[rng.uniform(-2,2) for _ in range(36)]);data=state+poses;inputs.append(data);o.mem_write(B,data);x.mem_write(B,data)
 for address,offset,size in [(0x18186c8,0,36),(0x1818690,36,12),(0x1818a38,48,36),(0x1818a28,84,12)]:o.mem_write(address,state[offset:offset+size])
 o.mem_write(0x1818b84,w(0));o.mem_write(0x5a4d18,b'\x13');x.mem_write(OWNER,state+w(FACE,0,3)+b'\x13'+bytes(3))
 for step in range(6):
  args=[B+96+step*48,B+108+step*48] if step<3 else []
  o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x5473f0 if step<3 else 0x547540,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
  expected=b''.join(bytes(o.mem_read(a,n)) for a,n in [(0x18186c8,36),(0x1818690,12),(0x1818a38,36),(0x1818a28,12)])+bytes(o.mem_read(0x1818b84,4))+bytes(o.mem_read(0x5a4d18,1))
  status=call('rf_visibility_view_push' if step<3 else 'rf_visibility_view_pop',[OWNER,*args]);got=bytes(x.mem_read(OWNER,96))+bytes(x.mem_read(OWNER+100,4))+bytes(x.mem_read(OWNER+108,1))
  assert status==0 and got==expected,(case,step,[(i,got[i:i+4].hex(),expected[i:i+4].hex()) for i in range(0,100,4) if got[i:i+4]!=expected[i:i+4]])
  if step<3:assert bytes(x.mem_read(FACE+step*96,96))==bytes(o.mem_read(0x18183c0+step*96,96))
  responses.append(w(0)+expected)
 assert got[:96]==state
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--view-stack'],input=b''.join(inputs))==b''.join(responses)
# Stack underflow, overflow and nonfinite pose leave all retained bytes intact.
for depth,invalid in [(0,False),(3,False),(0,True)]:
 x.mem_write(OWNER,state+w(FACE,depth,3)+b'\x13'+bytes(3));x.mem_write(FACE,b'\xa5'*288);x.mem_write(B,data)
 if invalid:x.mem_write(B+96,f(float('nan')))
 before=bytes(x.mem_read(OWNER,112))+bytes(x.mem_read(FACE,288))
 status=call('rf_visibility_view_pop',[OWNER]) if depth==0 and not invalid else call('rf_visibility_view_push',[OWNER,B+96,B+108])
 assert status and before==bytes(x.mem_read(OWNER,112))+bytes(x.mem_read(FACE,288))
report=dict(result='PASS',original_pc_nxdk_sequences=1024,steps=6144,nxdk_guards=3,scope='Unhooked original5473f0/547540 and all matrix/vector callees. Three nested pushes and pops, exact render/light transforms, saved96-byte slots, depth and color marker match PC/NXDK. Caller capacity is explicit; renderer-mode wrappers/native integration excluded.')
(root/'artifacts/view-stack.json').write_text(json.dumps(report,indent=2));print(report)

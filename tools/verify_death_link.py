"""Original4204a1..4205e8 linked-actor death stage vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
b=0x30000000;stack=b+0x1d000;stop=b+0x1e000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,0x20000);return m
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_death_link_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def ret(m,value):
 sp=m.reg_read(UC_X86_REG_ESP);target=get(m,sp);m.reg_write(UC_X86_REG_EAX,value&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,target)
def address(index,native):return (b+0x1000+(index-1)*0x100 if native else b+index*0x2000) if index else 0
# Wire offsets correspond to explicit original fields, not a copied actor ABI.
offsets=[0x2c,0x200,0x814,0x34,*range(0x3c,0x48,4),*range(0x48,0x6c,4),*range(0xe4,0xf0,4),*range(0xfc,0x120,4),0x7c0,0x7c4,0x69c,0x6a0]
assert len(offsets)==32
trace=[];facts=[]
def operate(m,op,index,arg,native):
 trace.extend((op,index,arg));actor=address(index,native)
 if op==9:return address(index,native) if 1<=index<=5 and facts[4]&(1<<index) else 0
 if op==0:return facts[0]
 if op==1:
  off=16 if native else 0x3c;v=struct.unpack('<f',m.mem_read(actor+off,4))[0];m.mem_write(actor+off,f(v+1))
 if op==2:
  m.mem_write(actor+(120 if native else 0x69c),w(123,456))
 if op==4 and facts[5]==1:m.mem_write(b+0x6100 if native else 0x5cb054,w(0))
 if op==6:return facts[2] if index else 0
 if op==7 and facts[5]==2:m.mem_write(address(3,native)+(12 if native else 0x34),w(0xabcdef))
 if op==8:return (facts[3]>>(index-3))&1
 return 0
ops={0x42a910:0,0x48a660:1,0x40c2c0:2,0x489f70:3,0x409050:4,0x408ac0:5,0x4290d0:6,0x4279d0:7,0x40a210:8,0x426fc0:9}
def hook(m,a,size,data):
 native=m is x;sp=m.reg_read(UC_X86_REG_ESP)
 if native:
  if a==b+0x7000:op=9;index=get(m,sp+8);arg=0
  elif a==b+0x7100:
   op=get(m,sp+8);ptr=get(m,sp+12);index=get(m,ptr) if ptr else 0;arg=get(m,sp+16)
  else:return
 else:
  if a not in ops:return
  op=ops[a];ptr=get(m,sp+4);arg=0
  if op==9:index=ptr
  else:
   if op in (4,5):ptr-=0x2a0
   if op==2:
    ptr-=0x3c
    assert get(m,sp+8)==get(m,ptr+0x7c0) and get(m,sp+12)==get(m,ptr+0x7c4)
    assert [get(m,sp+j) for j in (16,20,24,28)]==[0,ptr+0x69c,ptr+0x6a0,1]
   if op==3:assert get(m,sp+8)==0
   if op==4:arg=get(m,sp+8)
   index=get(m,ptr+0x2c) if ptr else 0
 ret(m,operate(m,op,index,arg,native))
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4204a1);commands=[];expected=[];active=0
for case in range(1024):
 source=w(rng.choice([0xffffffff,1,1,1]))+f(*[rng.randrange(-20,21)/4 for _ in range(12)])
 records=[w(i,3,rng.getrandbits(32),rng.getrandbits(32))+f(*[rng.randrange(-20,21)/4 for _ in range(26)])+w(70,80) for i in range(1,6)]
 assert all(len(r)==128 for r in records)
 facts=[rng.choice([0,0,0,256,1,255]),rng.choice([0,2]),rng.choice([0,256,1,255]),rng.randrange(8),rng.choice([62,62,60,54]),case%3,3]
 commands.append(source+b''.join(records)+w(*facts));trace=[]
 u.mem_write(b,bytes(0x1c000));u.mem_write(b+0x146c,source[:4]);u.mem_write(b+0x3c,source[4:]);u.mem_write(stack,bytes(0x200))
 for i,record in enumerate(records,1):
  for j,off in enumerate(offsets):u.mem_write(address(i,False)+off,record[j*4:j*4+4])
  u.mem_write(address(i,False)+0x28c,w(address(i+1,False) if 3<=i<5 else 0x5cb060))
 u.mem_write(0x5cb054,w(address(facts[1],False)));u.mem_write(0x5cb2ec,w(address(3,False)));before=bytes(u.mem_read(b,0xc000))
 u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EBP,0xffffffff);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4204a1,0x4205e8,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==0x4205e8
 result=b''.join(bytes(u.mem_read(address(i,False)+off,4)) for i in range(1,6) for off in offsets)
 ptr=get(u,0x5cb054);local=get(u,ptr+0x2c) if ptr else 0
 want=w(0)+result+w(local,len(trace)//3,*trace)+bytes((96-len(trace))*4);assert len(want)==1036
 expected.append(want);active+=int(1 in trace[::3])
 unchanged=bytearray(u.mem_read(b,0xc000))
 for i in range(1,6):
  for off in offsets:
   j=address(i,False)+off-b;unchanged[j:j+4]=before[j:j+4]
 assert bytes(unchanged)==before
 trace=[];x.mem_write(b,source)
 for i,record in enumerate(records,1):x.mem_write(address(i,True),record+w(address(i+1,True) if 3<=i<5 else 0))
 x.mem_write(b+0x6000,w(b+0x7000,b+0x7100,0,b+0x6100,b+0x6104,3));x.mem_write(b+0x6100,w(address(facts[1],True),address(3,True)))
 x.mem_write(stack,w(stop,b,b+0x6000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 ptr=get(x,b+0x6100);local=get(x,ptr) if ptr else 0
 got=w(x.reg_read(UC_X86_REG_EAX))+b''.join(bytes(x.mem_read(address(i,True),128)) for i in range(1,6))+w(local,len(trace)//3,*trace)+bytes((96-len(trace))*4)
 assert got==want,('NXDK',case,[(j,got[j:j+4].hex(),want[j:j+4].hex()) for j in range(0,len(want),4) if got[j:j+4]!=want[j:j+4]])
exe=root/'build/pc/Release/rf_entity_probe.exe';pc=subprocess.check_output([str(exe),'--death-link'],input=b''.join(commands));assert len(pc)==len(expected)*1036,(len(pc),len(expected)*1036)
for i,want in enumerate(expected):
 got=pc[i*1036:(i+1)*1036]
 assert got==want,('PC',i,[(j,got[j:j+4].hex(),want[j:j+4].hex()) for j in range(0,1036,4) if got[j:j+4]!=want[j:j+4]])
report=dict(result='PASS',cases=len(commands),handoffs=active,original_sha256=digest,pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original4204a1..4205e8 vs compiled PC/NXDK. Exact poses, room results, flags, visibility words, mutable local-player owner and callback ordering. Resource/registry/predicate callbacks supplied; original pose-copy helpers execute. No live death dispatch or XEMU gameplay claim.')
(root/'artifacts/death-link.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Original489fe0 model attachment versus shared PC/NXDK, supplied resources."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
r=lambda b,o=0:struct.unpack_from('<I',b,o)[0]
B=0x30000000;O=B;N=B+0x1000;BE=B+0x2000;CB=B+0x50000;S=B+0xe0000;STOP=S+0x1000
original=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x100000);return u
u=machine(original);x=machine(ROOT/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_object_model_attach\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
x.mem_write(CB,b'\xc3'*64);x.mem_write(BE,w(CB,CB+16,CB+32,CB+48,0))
trace=[];input_data=b'';shared=False
def text(cpu,a):
 v=bytearray()
 while cpu.mem_read(a,1)!=b'\0':v+=cpu.mem_read(a,1);a+=1
 return bytes(v)
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(cpu.mem_read(sp+4+i*4,4));result=0
 op=(address-CB)//16+1 if shared else {0x502880:1,0x5029c0:1,0x502a60:1,0x5032d0:2,0x503390:3,0x503210:4}[address]
 fail=r(input_data,108);kind=r(input_data,80);model=r(input_data,84)
 vals=[0]*4;name=b''
 if op==1:
  if shared:actualkind=arg(1);name=text(cpu,arg(2));vals[:3]=[actualkind,arg(3),arg(4)];out=arg(5)
  else:
   actualkind={0x502880:1,0x5029c0:2,0x502a60:3}[address];name=text(cpu,arg(0));vals[:3]=[actualkind,arg(1),arg(2) if actualkind!=3 else 0]
  assert actualkind==kind
  if fail!=op:
   if shared:cpu.mem_write(out,w(model))
   else:result=model
 elif op==2:
  vals[0]=arg(1) if shared else arg(0)
  if fail!=op:cpu.mem_write(arg(2) if shared else arg(1),input_data[88:100]);cpu.mem_write(arg(3) if shared else arg(2),input_data[100:104])
 elif op==3:vals[:3]=[arg(1),arg(2),arg(3)] if shared else [arg(0),arg(1),arg(2)]
 else:
  vals[0]=arg(1) if shared else arg(0)
  if fail!=op:
   if shared:cpu.mem_write(arg(2),input_data[104:108])
   else:result=r(input_data,104)
 trace.append(w(op,*vals)+name.ljust(64,b'\0'))
 if shared and fail==op:result=0xffffffff
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,arg(-1))
for a in (0x502880,0x5029c0,0x502a60,0x5032d0,0x503390,0x503210):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a in (CB,CB+16,CB+32,CB+48):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(cpu,entry,*args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(entry,STOP,count=1000000)
 assert cpu.reg_read(UC_X86_REG_EIP)==STOP;return cpu.reg_read(UC_X86_REG_EAX)
commands=[];expected=[];rng=random.Random(0x489fe0)
names=[b'miner.v3m',b'a.b.vfx',b'bare',b'.hidden',b'folder/a.v3m',b'x.',b'']
for case in range(512):
 kind=(1,2,3,0,4)[case%5];old=0 if case%3==0 else 0x12340000;loaded=0 if case%7==0 else 0x56780000
 center=[rng.randint(-64,64)/4 for _ in range(3)];radius=rng.randint(0,40)/4
 input_data=w(old,99)+f(23)+w(-7)+names[case%len(names)].ljust(64,b'\0')+w(kind,loaded)+f(*center,radius)+w(case,0)
 initial=bytearray(b'\xa5'*728)
 for offset,data in ((0x80,input_data[:4]),(0x84,input_data[4:8]),(0x78,input_data[8:12]),(0x26c,input_data[12:16])):initial[offset:offset+4]=data
 u.mem_write(O,bytes(initial));u.mem_write(N,input_data[16:80]);trace=[];shared=False
 result=call(u,0x489fe0,O,N,kind);original_trace=trace[:]
 final=bytes(u.mem_read(O,728));state=final[0x80:0x88]+final[0x78:0x7c]+final[0x26c:0x270]
 for off in range(728):
  if not any(a<=off<a+4 for a in (0x80,0x84,0x78,0x26c)):assert final[off]==initial[off]
 assert result==r(state)
 x.mem_write(O,input_data[:16]);x.mem_write(N,input_data[16:80]);trace=[];shared=True
 assert call(x,entry,O,N,kind,BE)==0
 actual=bytes(x.mem_read(O,16));assert actual==state,(case,actual.hex(),state.hex())
 assert trace==original_trace,(case,trace,original_trace)
 commands.append(input_data);expected.append(w(0)+state+w(len(trace))+b''.join(trace))
# Port service errors retain partial state for explicit caller cleanup.
for fail in range(1,5):
 input_data=w(0x12340000,99)+f(23)+w(-7)+b'a.vfx'.ljust(64,b'\0')+w(3,0x56780000)+f(3,4,0,2)+w(17,fail)
 x.mem_write(O,input_data[:16]);x.mem_write(N,input_data[16:80]);trace=[];shared=True
 status=call(x,entry,O,N,3,BE);assert status==0xffffffff and len(trace)==fail
 state=bytes(x.mem_read(O,16));wanted=input_data[:16] if fail==1 else w(0x56780000,-1)+f(23 if fail==2 else 7)+w(-7)
 assert state==wanted
 commands.append(input_data);expected.append(w(status)+state+w(len(trace))+b''.join(trace))
# Bounded port names and non-finite supplied bounds reject without unsafe access.
for guard in range(3):
 name=b'x'*64 if guard==0 else b'a.v3m'.ljust(64,b'\0')
 center=[float('nan'),0,0] if guard==1 else [0,0,0]
 radius=float('inf') if guard==2 else 2
 input_data=w(0x12340000,99)+f(23)+w(-7)+name+w(1,0x56780000)+f(*center,radius)+w(17,0)
 x.mem_write(O,input_data[:16]);x.mem_write(N,input_data[16:80]);trace=[];shared=True
 status=call(x,entry,O,N,1,BE);assert status==0xfffffffc and len(trace)==(0 if guard==0 else 2)
 state=bytes(x.mem_read(O,16))
 if guard==0:assert state==input_data[:16]
 else:assert state==w(0x56780000,-1)+f(radius)+w(-7)
 commands.append(input_data);expected.append(w(status)+state+w(len(trace))+b''.join(trace))
wire=w(len(commands))+b''.join(commands)
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--model-attach'],input=wire)
assert actual==b''.join(expected)
report=dict(result='PASS',original_cases=512,service_failures=4,port_guards=3,original_sha256=digest,scope='Full489fe0, real48a100 no-op, filename copy/last-dot removal and radius norm vs shared PC/NXDK. Supplied model loaders, bounds, animation and property callbacks. Exact state and ordered calls; unknown kinds retain incoming model. Port errors preserve partial owned state. No resource loader implementation or native XEMU claim.')
(ROOT/'artifacts/model-attach.json').write_text(json.dumps(report,indent=2));print(report)

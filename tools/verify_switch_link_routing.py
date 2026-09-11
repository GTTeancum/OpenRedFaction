"""Original Switch linked routing versus shared PC/NXDK callback traces."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
lookups=[0x4c08e0,0x46afa0,0x45afe0,0x45d5e0,0x4b6800,0x40a0e0]
effects={0x4c0200:(0,1),0x4c0210:(0,0),0x46aba0:(1,1),0x46b5b0:(1,0),0x45b040:(2,1),0x45b010:(2,0),0x45fb90:(3,None),0x4bd8b0:(4,1),0x4bd8a0:(4,0),0x4b8b70:(4,None),0x48a660:(5,1),0x48a570:(5,0)}
trace=[]
def hook(m,a,size,context):
 if a not in lookups and a not in effects:return
 sp=m.reg_read(UC_X86_REG_ESP);pop=0
 if a in lookups:
  family=lookups.index(a);link=read(sp+4);assert link in (100,200)
  mask=masks[link//100-1];target=base+0x2000+family*0x800+(link//100-1)*0x400
  trace.append(['lookup',family,link]);m.reg_write(UC_X86_REG_EAX,target if mask&(1<<family) else 0)
 else:
  family,on=effects[a]
  target=m.reg_read(UC_X86_REG_ECX) if a in (0x45fb90,0x4b8b70) else read(sp+4)
  if family in (1,2):link=target-1000
  else:link=100*((target-(base+0x2000+family*0x800))//0x400+1)
  assert link in (100,200),(hex(a),hex(target))
  if a==0x45fb90:on=read(sp+4);pop=4
  if a==0x4b8b70:
   assert [read(sp+4),read(sp+8)]==[77,88];on=read(sp+12);pop=12
  if a==0x46aba0:assert [read(sp+8),read(sp+12)]==[77,88]
  trace.append(['effect',family,link,on,'flags' if a in (0x4bd8a0,0x4bd8b0) else 'activate'])
 m.reg_write(UC_X86_REG_EIP,read(sp));m.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,hook)
entry=int(re.search(r'_rf_event_switch_links\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
lookup_address=base+0xc000;dispatch_address=base+0xc010;native_trace=[]
def shared_hook(m,address,size,context):
 if address not in (lookup_address,dispatch_address):return
 sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<5I',m.mem_read(sp,20));code=0
 if address==lookup_address:
  _,ctx,family,link,target=args;native_trace.append(['lookup',family,link])
  if not (masks[link//100-1]&(1<<family)):code=0xfffffffd
  else:m.mem_write(target,w(link,17 if damage else 51,renderable))
 else:
  family,link,on,source,actor,flags=struct.unpack('<6I',m.mem_read(args[2],24))
  assert (source,actor)==(77,88)
  native_trace.append(['effect',family,link,on,'flags' if flags else 'activate'])
 m.reg_write(UC_X86_REG_EAX,code);m.reg_write(UC_X86_REG_EIP,args[0]);m.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,shared_hook)
commands=bytearray();results=bytearray()
cases=0;effect_counts=[0]*6
import itertools
for disabled,initial,mask,damage,renderable in itertools.product((0,1,2),(0,1,256,257),range(64),(0,1),(0,1)):
 masks=[mask,63^mask];u.mem_write(base,bytes(0x300));u.mem_write(base+0x29c,w(2,2,base+0x1000));u.mem_write(base+0x1000,w(100,200));u.mem_write(base+0x2a8,w(88,77));u.mem_write(base+0x2b8,w(disabled))
 for family in range(6):
  for i,link in enumerate((100,200)):
   target=base+0x2000+family*0x800+i*0x400;u.mem_write(target,bytes(0x300))
   if family==1:u.mem_write(target+0x2c,w(1000+link))
   if family==2:u.mem_write(target+8,w(1000+link))
   if family==4:u.mem_write(target+0x290,w(17 if damage else 51))
   if family==5:u.mem_write(target+0x80,w(renderable))
 trace=[];u.mem_write(stack,w(stop,initial));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
 u.emu_start(0x4bc340,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected=[]
 for i,link in enumerate((100,200)):
  early=False
  for family in range(4):
   expected.append(['lookup',family,link])
   if masks[i]&(1<<family):
    if family!=3 or disabled in (0,1):expected.append(['effect',family,link,int(disabled==0),'activate'])
    early=True;break
  if early:continue
  expected.append(['lookup',4,link])
  if masks[i]&16 and (damage or not(initial&255)):expected.append(['effect',4,link,int(disabled==0),'flags' if damage else 'activate'])
  expected.append(['lookup',5,link])
  if masks[i]&32 and renderable:expected.append(['effect',5,link,int(disabled==0),'activate'])
 assert trace==expected,(disabled,initial,mask,damage,renderable,trace,expected)
 x.mem_write(base,w(disabled,0,0,0,0));x.mem_write(base+0x100,w(32,0,0xffffffff,88,77,0,0));x.mem_write(base+0x200,w(2,base+0x300));x.mem_write(base+0x300,w(100,200))
 x.mem_write(stack,w(stop,base,base+0x100,base+0x200,initial,lookup_address,dispatch_address,0));x.reg_write(UC_X86_REG_ESP,stack);native_trace=[]
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert native_trace==trace,(cases,native_trace,trace)
 code=2166136261
 for row in trace:
  values=[0,row[1],row[2]] if row[0]=='lookup' else [1,row[1],row[2],row[3],int(row[4]=='flags')]
  for value in values:code=((code^value)*16777619)&0xffffffff
 commands.extend(w(disabled,initial,mask,damage,renderable));results.extend(w(0,code))
 for row in trace:
  if row[0]=='effect':effect_counts[row[1]]+=1
 cases+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--switch-links'],input=commands)
assert actual==results,'PC lookup/effect trace differs'
report=dict(result='PASS',cases=cases,effect_counts=effect_counts,original_sha256=digest,scope='Original4bc340 ordered linked routing and actual list callees. Six resolver boundaries supply independent synthetic target availability; downstream effects observed, not executed. Exact lookup/effect order, arguments, priority, dual event/object path, type17 special flag updates, initial low-byte suppression, renderable gating and disabled0/1/other. Shared PC/NXDK ordered traces and source/actor arguments match; campaign owner integration remains separate.')
(root/'artifacts/switch-link-routing.json').write_text(json.dumps(report,indent=2));print(report)

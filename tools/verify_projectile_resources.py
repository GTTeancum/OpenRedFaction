"""PC/NXDK projectile resource composition; supplied model services, real physics."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;RES=B+0x10000;REG=B+0x20000;LIST=B+0x24000;UID=LIST+64;D=LIST+128;NAME=D+256;MAT=NAME+128;BACK=MAT+64;OUT=BACK+64;SPHERES=OUT+64;STACK=B+0xfe000;STOP=B+0xff000;CALL=B+0xf0000
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(B,0x100000)
mapping=(root/'build/xbox/main.map').read_text()
sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
word=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
state=dict(mode=0,loads=0,releases=0,live=0,heap={});malloc=sym('malloc');calloc=sym('calloc');free=sym('free')
def hook(c,a,size,data):
 sp=c.reg_read(UC_X86_REG_ESP);args=[word(sp+4+i*4) for i in range(6)];mode=state['mode'];result=0
 if a in (malloc,calloc):
  amount=args[0]*(args[1] if a==calloc else 1);assert amount in (24,48)
  pointer=B+0x40000
  if state.get('fail')==a:
   c.reg_write(UC_X86_REG_EAX,0);c.reg_write(UC_X86_REG_EIP,word(sp));c.reg_write(UC_X86_REG_ESP,sp+4);return
  while pointer in state['heap']:pointer+=0x1000
  state['heap'][pointer]=amount;x.mem_write(pointer,bytes([0 if a==calloc else 0xa5])*amount);result=pointer
 elif a==free:
  if args[0]:assert args[0] in state['heap'];del state['heap'][args[0]]
 elif a==CALL:
  state['loads']+=1;assert args[1]==1 and args[3:5]==[1,0xffffffff]
  assert bytes(x.mem_read(args[2],11))==b'rocket.v3m\0'
  if mode==7:result=-1
  else:
   model=0 if mode==6 else 123;x.mem_write(args[5],w(model));state['live']+=bool(model)
 elif a==CALL+16:
  assert args[1]==123
  if mode==8:result=-1
  else:x.mem_write(args[2],f(0,0,0));x.mem_write(args[3],f(3))
 elif a==CALL+32:raise AssertionError('unexpected animation')
 elif a==CALL+48:assert args[1]==123;x.mem_write(args[2],w(9))
 elif a==CALL+64:
  assert args[1]==123
  if mode==9:result=-1
  else:
   count=1 if mode==4 else 2 if mode==5 else 0
   x.mem_write(args[2],w(SPHERES,count,1,0,0))
 elif a==CALL+80:
  assert args[1]==123 and state['live']==1;state['live']-=1;state['releases']+=1
 else:raise AssertionError(hex(a))
 c.reg_write(UC_X86_REG_EAX,result&0xffffffff);c.reg_write(UC_X86_REG_EIP,word(sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for a in [malloc,calloc,free,*range(CALL,CALL+96,16)]:x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP,(name,hex(x.reg_read(UC_X86_REG_EIP)))
 return x.reg_read(UC_X86_REG_EAX)
run('rf_projectile_store_init',[B]);run('rf_object_registry_init',[REG]);run('rf_object_list_init',[LIST]);x.mem_write(UID,w(1000))
x.mem_write(NAME,b'rocket.v3m\0');x.mem_write(MAT,f(.25,.5,2));x.mem_write(BACK,w(CALL,CALL+16,CALL+32,CALL+48,0,CALL+64,CALL+80))
x.mem_write(SPHERES,bytes(32)+f(2,0,0,1.5)+bytes(32)+f(-2,0,0,.5))
expected=bytearray();statuses=[]
for mode in range(12):
 state['mode']=mode;desc=bytearray(156);desc[:8]=w(int(mode>=3),1);desc[20:24]=f(2)
 desc[72:108]=f(1,0,0,0,1,0,0,0,1);desc[108:132]=f(1,2,3,4,5,6)
 desc[132:136]=f(0 if mode==1 else 2 if mode==2 else -1);desc[148:152]=w(0x80000870)
 if mode==11:desc[108:112]=w(0x7fc00000)
 budget=348+24+(24 if mode==4 else 72 if mode==5 else -1 if mode==10 else 0)
 x.mem_write(D,bytes(desc));x.mem_write(OUT,w(0))
 status=run('rf_projectile_initialized_open',[B,RES,REG,LIST,UID,D,NAME if mode>=3 else 0,MAT,BACK,budget,OUT])
 statuses.append(status);expected+=w(status)
 assert status==([0]*6+[0xfffffffd]+[0xffffffff]*3+[0xfffffffc]*2)[mode],(mode,status)
 if not status:
  owner=word(OUT);handle=word(owner+4);slot=word(owner+12);r=RES+slot*348
  assert word(REG+(handle&65535)*8)==owner and word(LIST+8)==1
  assert bytes(x.mem_read(r+16+184,12))==f(1,2,3) # represented body velocity
  assert bytes(x.mem_read(r+16+196,12))==f(4,5,6)
  assert bytes(x.mem_read(r+16+208,12))==f(8,10,12)
  count=word(r+328);assert count==(2 if mode==5 else 1)
  pointer=word(r+324);spheres=bytes(x.mem_read(pointer,count*24))
  if mode==4:assert spheres[:16]==f(0,0,0,1.5)
  if mode in (0,1,2,3):assert spheres[12:16]==f((1,0,2,3)[mode])
  expected+=bytes(x.mem_read(r,324))+w(count)+bytes(x.mem_read(r+340,8))+spheres
  x.mem_write(owner+16,w(word(owner+16)|2));assert word(r+340)
  assert run('rf_projectile_initialized_close',[B,RES,REG,LIST,handle,BACK])==0
  assert bytes(x.mem_read(r,348))==bytes(348)
 else:assert word(OUT)==0
 assert bytes(x.mem_read(D,156))==desc and not state['heap'] and state['live']==0
 assert word(LIST+8)==word(B+39408)==0 and word(REG+12292)==1024
assert state['loads']==9 and state['releases']==7 and word(UID)==988
pc=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--projectile-resources'])
assert pc==expected,('PC/NXDK resource mismatch',len(pc),len(expected),[(i,pc[i:i+4].hex(),expected[i:i+4].hex()) for i in range(0,len(pc),4) if pc[i:i+4]!=expected[i:i+4]][:15])
# Failure in temporary sphere calloc and retained sphere malloc must unwind
# the registered slot and the successfully transferred model exactly once.
state['mode']=4;desc[108:112]=f(1);x.mem_write(D,bytes(desc))
for allocation in (calloc,malloc):
 state['fail']=allocation;before=state['releases'];x.mem_write(OUT,w(0))
 assert run('rf_projectile_initialized_open',[B,RES,REG,LIST,UID,D,NAME,MAT,BACK,396,OUT]) in (0xffffffff,0xfffffffc)
 assert word(OUT)==0 and state['releases']==before+1 and not state['heap'] and not state['live']
 assert word(LIST+8)==word(B+39408)==0 and word(REG+12292)==1024
 assert bytes(x.mem_read(RES,348*50))==bytes(348*50)
state.pop('fail')
report=dict(result='PASS',cases=12,nxdk_allocation_failures=2,resource_owner_bytes=348,loads=9,releases=7,statuses=statuses,scope='PC and compiled NXDK pool/list/registry composed with actual model attachment, sphere conversion and moving physics; model/heap services supplied. Missing models, load/bounds/sphere errors, short budget and invalid motion release resources; descriptor preserved, mark does not close. Does not verify full original factory, model backend storage budget, live scene scheduling, effects or native XEMU.')
(root/'artifacts/projectile-resources.json').write_text(json.dumps(report,indent=2));print(report)

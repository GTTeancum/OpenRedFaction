"""Compare shared PC/NXDK corpse deletion with complete original type7 traces."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
cases=[];expected=[];handle=0x80007
def observe(g):
 flags,model,burn,sid,found,count=[g[k] for k in ('flags','model','burn','sid','found','count')]
 cases.append([flags,model,burn,sid,int(found),count,0]);trace=[]
 ops={0x48c9f0:0,0x42ed20:2,0x49f1d0:3,0x502b10:4,0x497d80:5,0x459a20:8}
 for addr,ecx,args in g['trace']:
  op=(1 if ecx==g['actor']+0x2a4 else 6) if addr==0x4ffa80 else ops[addr]
  token=(100+g['emitters'].index(args[0])) if op==5 else args[0] if op in (2,4,8) else handle
  trace.extend((op,token))
 trace.extend((7,handle))
 out=[0,0xffffffff if found else sid,0x502 if found else 0x500,burn,12,78,2,0,1017,7,0,0,0,0,len(trace)//2,0]+trace
 expected.append(out+[0]*(40-len(out)))
runpy.run_path(str(root/'tools/verify_corpse_delete_original.py'),init_globals={'observe_case':observe})
original_count=len(cases)
for mode in (1,2,3,4):
 row=[0,44,55,66,1,4,mode];cases.append(row)
 expected.append([(-3 if mode==2 else -4)&0xffffffff,66,0x500,55,13,79,1 if mode==3 else 0,1,1016,0,1,1,1,0,0,0]+[0]*24)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-delete'],input=b''.join(w(*c) for c in cases))
assert len(pc)==160*len(cases)
path=root/'build/xbox/main.exe';p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;x.mem_map(b,65536);state=b;update=b+0x100;ch=b+0x200;oh=b+0x220;emit=b+0x300;counts=b+0x400;sound=b+0x420
backend=b+0x500;registry=b+0x1000;effect=b+0xb000;soundfn=b+0xb010;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'_rf_corpse_delete\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
def put(a,v):x.mem_write(a,w(v))
trace=[];errors=0;found=0;recycled=False
def read_hook(machine,access,address,size,value,unused):
 assert not (recycled and (state<=address<state+40 or update<=address<update+88)),('owner read after recycle',hex(address))
x.hook_add(UC_HOOK_MEM_READ,read_hook)
def hook(machine,address,size,unused):
 global errors,recycled
 if address not in (effect,soundfn):return
 esp=machine.reg_read(UC_X86_REG_ESP)
 if read(registry+7*8)!=state:errors+=1
 if address==soundfn:
  trace.extend((8,read(esp+8)));machine.reg_write(UC_X86_REG_EAX,sound if found else 0)
 else:
  op,token=read(esp+8),read(esp+12);trace.extend((op,token))
  if op==5:
   e=read(state+20)
   if not e or read(e+4)!=token:errors+=1
   machine.mem_write(e,bytes([0xdd])*8)
  if op==3 and (read(counts)!=12 or read(state+4)):errors+=1
  if op==7 and (read(counts+4)!=78 or read(state+12) or read(state+32)!=2):errors+=1
  if op==7:recycled=True
 machine.reg_write(UC_X86_REG_ESP,esp+4);machine.reg_write(UC_X86_REG_EIP,read(esp))
x.hook_add(UC_HOOK_CODE,hook)
for i,(row,want) in enumerate(zip(cases,expected)):
 assert pc[i*160:(i+1)*160]==w(*want),('PC',i,struct.unpack('<40I',pc[i*160:(i+1)*160]),want)
 flags,model,burn,sid,found,count,mode=row;trace.clear();errors=0;recycled=False;x.mem_write(b,bytes(0xa000))
 put(update+8,flags);put(update+28,model);put(update+36,sid);put(sound,0x500)
 x.mem_write(state,w(update,ch,ch,oh,oh,emit if count else 0,burn,handle^(0x10000 if mode==2 else 0),1 if mode==3 else 0,state))
 x.mem_write(ch,w(ch if mode==4 else state+4,state+4));x.mem_write(oh,w(state+12,state+12))
 for j in range(count):x.mem_write(emit+j*8,w(emit+(j+1)*8 if j+1<count else emit if mode==1 else 0,100+j))
 x.mem_write(counts,w(13,79));x.mem_write(backend,w(effect,soundfn,0))
 for j in range(1024):put(registry+8192+j*4,j)
 for j in range(8):x.mem_write(registry+j*8,w(state if j==7 else b+0x800+j*4,((j+1)<<16)|j))
 x.mem_write(registry+12288,w(8,1016,9))
 x.mem_write(stack,w(stop,state,registry,counts,counts+4,4,backend));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 got=[x.reg_read(UC_X86_REG_EAX),read(update+36),read(sound),read(state+24),read(counts),read(counts+4),read(state+32),int(bool(read(registry+56))),read(registry+12292),read(registry+8192),int(bool(read(state+20))),int(bool(read(state+4))),int(bool(read(state+12))),errors,len(trace)//2,0]+trace
 got+=[0]*(40-len(got));assert got==want,('NXDK',i,got,want)
report=dict(result='PASS',original_cases=original_count,guard_cases=len(cases)-original_count,nxdk_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),scope='Complete original type7 deletion traces compared to shared PC/NXDK orchestration, real shared registry removal and lists. Supplied resource/pool callbacks; stale/reentrant/broken-link/cycle guards. No live corpse allocator or native XEMU invocation.')
(root/'artifacts/corpse-delete-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

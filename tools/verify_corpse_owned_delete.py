"""Execute concrete corpse deletion ownership in PC and NXDK machine code."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
print(subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-owned-delete'],text=True).strip())
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v));f=lambda *v:struct.pack('<'+'f'*len(v),*v)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);b=0x30000000;u.mem_map(b,0x40000)
c=b+136;body=c+276;names=c+620;registry=b+0x8000;seed=b+0xc000;out=seed+128;ch=b+0xc200;oh=b+0xc220;counts=b+0xc240;sound=b+0xc260;emitters=b+0xc300
backend=b+0xd000;effect=b+0xd100;sound_cb=b+0xd110;text=b+0xd200;stack=b+0x3e000;stop=b+0x3f000;base=19224
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
init=sym('rf_corpse_owners_init');reginit=sym('rf_object_registry_init');acquire=sym('rf_corpse_base_acquire');assign=sym('rf_corpse_name_assign');delete=sym('rf_corpse_owned_delete');release=sym('rf_corpse_pool_release');remove=sym('rf_object_registry_remove');malloc=sym('malloc');free=sym('free')
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
live={};labels={};trace=[];deleting=False;retired=False;handle=0

def hook(cpu,address,size,data):
 global retired
 if deleting and address==remove:
  assert retired and read(b+124)==0;trace.append(('registry_remove',handle));return
 if address not in (malloc,free,effect,sound_cb):return
 sp=cpu.reg_read(UC_X86_REG_ESP);a=read(sp+4)
 if address==malloc:
  assert not deleting and 0<a<=24
  pointer=next(b+0x10000+256*i for i in range(3) if b+0x10000+256*i not in live);live[pointer]=a
  cpu.mem_write(pointer,bytes([0xa5])*a);cpu.reg_write(UC_X86_REG_EAX,pointer)
 elif address==free:
  if a:
   assert deleting and a in live and read(registry)==c and not retired
   trace.append(('free',labels[a]));del live[a];cpu.mem_write(a,bytes([0xdd])*24)
 elif address==sound_cb:
  sid=read(sp+8);assert a==123 and read(names+12)==0 and read(body+320)>0 and read(names+4)>0
  trace.append(('sound',sid));cpu.reg_write(UC_X86_REG_EAX,sound if case%2 else 0)
 else:
  op=read(sp+8);token=read(sp+12);assert a==123 and read(registry)==c and read(counts+4)==1
  assert op in (0,2,4,5);trace.append(({0:'pairs',2:'burn',4:'model',5:'emitter'}[op],token))
  if op==0:assert read(names+4)>0 and read(names+12)>0 and read(body+320)>0
  if op==2:assert read(names+12)==0 and read(body+320)>0 and read(counts)==1
  if op in (4,5):assert read(body+320)==0 and read(names+12)==0 and read(names+4)>0 and read(counts)==0
  if op==5:
   e=read(c+108);assert read(e+4)==token;cpu.mem_write(e,bytes([0xdd])*8)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
def recycled(cpu,access,address,size,value,data):
 global retired
 if deleting and address==b+124 and value==0:
  assert read(c+120)==2 and read(counts)==read(counts+4)==0 and read(registry)==c
  trace.append(('recycle',handle));retired=True

def memory(cpu,access,address,size,value,data):
 assert not (retired and address<c+636 and address+size>c),('owner read after recycle',hex(address))
u.hook_add(UC_HOOK_CODE,hook);u.hook_add(UC_HOOK_MEM_READ,memory);u.hook_add(UC_HOOK_MEM_WRITE,recycled)
def call(address,*args):
 u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(address,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
for case in range(256):
 deleting=retired=False;assert not live;labels.clear();trace.clear()
 assert call(init,b,base+128)==0;call(reginit,registry)
 u.mem_write(seed,f(10,3,0,0,0,1,0,0,0,1,0,0,0,1,1)+w(0,0,0x33));u.mem_write(oh,w(oh,oh));u.mem_write(counts,w(0,0))
 material=struct.unpack('<3I',f(.25,.5,2));assert call(acquire,b,registry,oh,counts+4,seed,*material,2,out)==0 and read(out)==0
 handle=read(c+116);u.mem_write(text,b'corpse\0');assert call(assign,b,0,0,text)==0
 u.mem_write(text,b'death_front\0');assert call(assign,b,0,1,text)==0
 labels.update({read(body+308):'body',read(names+4):'object',read(names+12):'death'})
 u.mem_write(c+92,w(ch,ch));u.mem_write(ch,w(c+92,c+92));u.mem_write(counts,w(1))
 u.mem_write(c+28,w(77 if case%3 else 0));u.mem_write(c+8,w(0x6400400 if case%2 else 0x6400000));u.mem_write(c+112,w(88 if case%4 else 0))
 sid=case if case%7 else 0xffffffff;u.mem_write(c+36,w(sid));u.mem_write(sound,w(0x500))
 n=case%5;u.mem_write(c+108,w(emitters if n else 0))
 for i in range(n):u.mem_write(emitters+8*i,w(emitters+8*(i+1) if i+1<n else 0,100+i))
 u.mem_write(backend,w(effect,sound_cb,123));args=(b,0,registry,counts,counts+4,4,backend)
 # A stale handle must reject before freeing any owned resource.
 u.mem_write(c+116,w(handle^0x10000));assert call(delete,*args)==0xfffffffd and len(live)==3 and not trace
 u.mem_write(c+116,w(handle));deleting=True;assert call(delete,*args)==0
 want=[('pairs',handle),('free','death'),('sound',sid)]
 if case%4:want.append(('burn',88))
 want.append(('free','body'))
 if case%3 and not case%2:want.append(('model',77))
 want.extend(('emitter',100+i) for i in range(n));want.extend([('free','object'),('recycle',handle),('registry_remove',handle)])
 assert trace==want,(case,trace,want)
 assert retired and not live and read(b+base-8)==base and read(b+124)==0 and read(registry)==0 and read(registry+12292)==1024
 assert read(sound)==(0x502 if case%2 else 0x500) and read(c+36)==(0xffffffff if case%2 else sid)
 assert bytes(u.mem_read(ch,8))==w(ch,ch) and bytes(u.mem_read(oh,8))==w(oh,oh)
 assert call(delete,*args)==0xfffffffc # inactive slot, no retired owner access
report=dict(result='PASS',cases=256,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Real owned names/body allocation, registered base and deletion bridge on PC/NXDK. Exact release/forwarded effect sequence, budget restoration, registry after recycle, poisoned emitter links, stale/reentrant/repeated rejection and no Xbox owner reads after recycle. Model/burn/emitter/pair/sound backends supplied; original sequence independently verified by verify_corpse_delete_original.py. No live scene/XEMU binding.')
(root/'artifacts/corpse-owned-delete-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

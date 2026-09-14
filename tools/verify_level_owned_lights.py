"""All installed v180 light records: independent inventory vs PC/NXDK reader."""
import ctypes as c,json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
class Light(c.Structure):
 _fields_=[(n,c.c_uint32) for n in ('offset','bytes','uid')]+[(n,c.c_char*256) for n in ('name','script')]+[('position',c.c_float*3),('orientation_disk',c.c_float*9),('header_byte',c.c_uint32),('flags',c.c_uint32),('color',c.c_uint8*4)]+[(n,c.c_float) for n in ('radius','inner_angle','outer_delta','cone_scale')]+[('profile',c.c_uint32),('length',c.c_float),('cycle',c.c_float*6)]
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);B=0x30000000;LEVEL=B+0x8000;OUT=B+0x1000;STACK=B+0xe000;STOP=B+0xf000
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(B,65536)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16);read_entry=sym('rf_vpp_read');data=b''
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,archive,entry,offset,dest,count=struct.unpack('<6I',cpu.mem_read(sp,24));offset-=8
 assert offset<=len(data) and count<=len(data)-offset
 if count:cpu.mem_write(dest,data[offset:offset+count])
 cpu.reg_write(UC_X86_REG_EAX,0);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook,begin=read_entry,end=read_entry)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(sym(name),STOP,count=100000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
HEAP=0x31000000;x.mem_map(HEAP,1024*1024);alloc=[];freed=[];fail=False
malloc=sym('malloc');free=sym('free')
def heap_hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',cpu.mem_read(sp,8))
 if address==malloc:
  assert arg<=1024*1024;alloc.append(arg);cpu.reg_write(UC_X86_REG_EAX,0 if fail else HEAP)
 elif arg:assert arg==HEAP;freed.append(arg)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,heap_hook,begin=malloc,end=malloc);x.hook_add(UC_HOOK_CODE,heap_hook,begin=free,end=free)
def prepare():
 x.mem_write(LEVEL,bytes(4096));x.mem_write(LEVEL,w(B+0x9000));x.mem_write(LEVEL+72,w(len(data)+8,180,0,1));x.mem_write(LEVEL+600,w(0x300,0,len(data)));x.mem_write(OUT,w(0));x.mem_write(B+0x2000,w(123))
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());report=json.loads((root/'artifacts/level-lights.json').read_text());total=0;peak=0;sample=None
for level in report['results']:
 description=next(l for l in levels if l['file']==level['file'] and l['archive']==level['archive']);section=next(s for s in description['sections'] if s['type']=='0x300');archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as stream:stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
 count=len(level['records']);budget=60+105600+160*count;peak=max(peak,budget)
 if sample is None and count>=2:sample=(data,level['records'],budget)
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-owned-lights',str(root/'Installed_Game'/level['archive']),level['file']])
 prepare();alloc.clear();freed.clear()
 assert call('rf_level_owned_lights_open',[LEVEL,budget-1,1,1,B+0x2000,OUT])!=0 and not alloc and x.mem_read(OUT,4)==w(0)
 assert call('rf_level_owned_lights_open',[LEVEL,budget,1,1,B+0x2000,OUT])==0 and alloc==[budget]
 assert x.mem_read(OUT,4)==w(HEAP)
 expected=bytes(x.mem_read(HEAP,8))+bytes(x.mem_read(HEAP+8,36))+bytes(x.mem_read(B+0x2000,4))+bytes(x.mem_read(HEAP+60,budget-60))
 assert raw==expected,(level['file'],len(raw),len(expected))
 # Independently derive initial clocks from activation, checking every field.
 for j in range(count):
  item=raw[48+j*136:48+(j+1)*136];activation=item[36:]
  clock=raw[48+count*136+105600+j*24:48+count*136+105600+(j+1)*24]
  assert clock==activation[88:92]+w(0)+activation[92:96]+activation[68:72]+w(0,0),(level['file'],j)
 # Bind selected pool IDs in reverse chunks, checking authored flags independently.
 for first in range(0,count,63):
  records=level['records'][first:first+63][::-1];indices=list(range(first,first+len(records)))[::-1]
  ids=[struct.unpack_from('<I',raw,48+j*136+4)[0] for j in indices]
  expected_modes=w(*[2 if r['flags']&0x2000 else (1 if r['flags']&4 else 0) for r in records])
  x.mem_write(B+0x3000,w(*ids));x.mem_write(B+0x4000,b'\xa5'*252)
  assert call('rf_level_owned_light_shadow_modes',[HEAP,B+0x3000,len(ids),B+0x4000,63])==0
  assert bytes(x.mem_read(B+0x4000,len(ids)*4))==expected_modes
  saved_modes=bytes(x.mem_read(B+0x4000,252))
  assert call('rf_level_owned_light_shadow_modes',[HEAP,B+0x3000,len(ids),B+0x4000,len(ids)-1])!=0 and bytes(x.mem_read(B+0x4000,252))==saved_modes
  assert call('rf_level_owned_light_shadow_modes',[HEAP,B+0x3000,len(ids),B+0x3000,63])==0 and bytes(x.mem_read(B+0x3000,len(ids)*4))==expected_modes
  x.mem_write(B+0x3000,w(1100))
  assert call('rf_level_owned_light_shadow_modes',[HEAP,B+0x3000,1,B+0x4000,63])!=0 and bytes(x.mem_read(B+0x4000,252))==saved_modes
 # Source/archive context can be destroyed; all retained storage stays intact.
 saved=bytes(x.mem_read(HEAP,budget));x.mem_write(LEVEL,b'\xa5'*4096);assert bytes(x.mem_read(HEAP,budget))==saved
 call('rf_level_owned_lights_close',[OUT])
 call('rf_level_owned_lights_close',[OUT]);assert x.mem_read(OUT,4)==w(0) and freed==[HEAP];total+=count
# Allocation failure and a late conversion failure preserve RNG/output.
data,records,budget=sample;prepare();fail=True;alloc.clear();freed.clear()
assert call('rf_level_owned_lights_open',[LEVEL,budget,1,1,B+0x2000,OUT])!=0 and x.mem_read(OUT,4)==w(0) and x.mem_read(B+0x2000,4)==w(123) and not freed
fail=False;bad=bytearray(data)
for j in (0,1):
 r=records[j];at=r['offset']+57+len(r['name'].encode('cp1252'))+len(r['script'].encode('cp1252'));flags=(r['flags']&~0xf00)|0x300
 if j==1:flags&=~0xf0
 bad[at:at+4]=w(flags)
data=bytes(bad);prepare();alloc.clear();freed.clear()
assert call('rf_level_owned_lights_open',[LEVEL,budget,1,1,B+0x2000,OUT])!=0 and x.mem_read(OUT,4)==w(0) and x.mem_read(B+0x2000,4)==w(123) and alloc==[budget] and freed==[HEAP]
result=dict(result='PASS',levels=len(report['results']),pc_nxdk_sources=total,selected_authored_shadow_modes=total,exact_and_short_budget_levels=len(report['results']),peak_owned_bytes=peak,nxdk_failure_guards=2,scope='PC actual archives and NXDK shared reader/activation/pool owner with only archive read and heap supplied. Entire retained runtime/pool payload matches, including after source-context destruction; repeated close and RNG/output rollback. Original component comparisons are separate; live scene/timer/visibility integration remains.')
(root/'artifacts/level-owned-lights.json').write_text(json.dumps(result,indent=2));print(result)

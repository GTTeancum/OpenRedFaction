"""Bounded v180 region reader across installed levels, PC and compiled NXDK.
Layout recovered from 462e40; this checks decoded bytes, not original loader execution.
"""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
from inspect_levels import inspect
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
binary=root/'build/xbox/main.exe';pe=pefile.PE(str(binary));image=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(image)+4095)//4096*4096);x.mem_write(origin,image)
base=0x30000000;x.mem_map(base,65536);stack=base+0xe000;stop=base+0xf000;output=base+0x1000
mapping=(root/'build/xbox/main.map').read_text();symbol=lambda name:int(re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry=symbol('rf_level_region_next');read_entry=symbol('rf_vpp_read');data=b''
owned_entry=symbol('rf_level_owned_regions_open');close_entry=symbol('rf_level_owned_regions_close')
malloc_entry=symbol('malloc');free_entry=symbol('free');heap=base+0xa000;allocations=[];frees=[]
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
def hook(cpu,address,size,context):
 if address in (malloc_entry,free_entry):
  sp=cpu.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',cpu.mem_read(sp,8))
  if address==malloc_entry:
   assert arg<=0x3000;allocations.append(arg);cpu.reg_write(UC_X86_REG_EAX,heap)
  elif arg:
   assert arg==heap;frees.append(arg)
  cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret);return
 if address!=read_entry:return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,level,section,offset,dest,count=struct.unpack('<6I',cpu.mem_read(sp,24))
 offset-=8
 assert offset<=len(data) and count<=len(data)-offset
 cpu.mem_write(dest,data[offset:offset+count]);cpu.reg_write(UC_X86_REG_EAX,0);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
levels=total=0;summary=[]
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for item in archive.get('vpp',{}).get('entries',[]):
  if not item['name'].lower().endswith('.rfl'):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as stream:
   level=inspect(stream,item);section=next((s for s in level['sections'] if s['type'].lower()=='0xd00'),None)
   if not section:continue
   stream.seek(item['offset']+section['offset']+8);data=stream.read(section['size'])
  cursor=0
  def take(n):
   global cursor
   assert n<=len(data)-cursor;v=data[cursor:cursor+n];cursor+=n;return v
  def string():return take(struct.unpack('<H',take(2))[0])
  count=struct.unpack('<I',take(4))[0];expected=[]
  for i in range(count):
   take(4);string();position=take(12);matrix=take(36);string();take(1);kind=take(4);size=take(12)
   expected.append(kind+position+matrix[12:]+matrix[:12]+size)
  assert cursor==len(data)
  probe=root/'build/pc/Release/rf_entity_probe.exe'
  actual=subprocess.check_output([str(probe),'--level-regions',str(path),item['name']])
  assert actual==b''.join(expected),item['name']
  owned=subprocess.check_output([str(probe),'--owned-level-regions',str(path),item['name']])
  assert owned==actual
  x.mem_write(base+0x8000,bytes(0x1000));x.mem_write(base+0x8000,w(base+0x9000))
  x.mem_write(base+0x8000+72,w(len(data)+8,180,0,1))
  x.mem_write(base+0x8000+600,w(0xd00,0,len(data)))
  budget=12+count*64;allocations.clear();frees.clear()
  for limit in (budget-1,budget):
   x.mem_write(output,bytes(12));x.mem_write(stack,w(stop,base+0x8000,limit,output));x.reg_write(UC_X86_REG_ESP,stack)
   x.emu_start(owned_entry,stop,count=100000)
   assert x.reg_read(UC_X86_REG_EIP)==stop
   if limit<budget:
    assert x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(output,12))==bytes(12) and not allocations
   else:
    assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(output,12))==w(heap if count else 0,count,budget)
    assert bytes(x.mem_read(heap,count*64))==actual and allocations==([count*64] if count else [])
  x.mem_write(base+0x8000,bytes([0xa5])*0x1000)
  assert bytes(x.mem_read(heap,count*64))==actual
  for _ in range(2):
   x.mem_write(stack,w(stop,output));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(close_entry,stop,count=10000)
   assert x.reg_read(UC_X86_REG_EIP)==stop and bytes(x.mem_read(output,12))==bytes(12)
  assert len(frees)==int(count>0)

  x.mem_write(base+0x8000,w(base+0x9000))
  x.mem_write(base+0x8000+72,w(len(data)+8))
  x.mem_write(base,w(base+0x8000,0xd00,0,len(data),4,count,0))
  for want in expected:
   x.mem_write(stack,w(stop,base,output));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
   assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,(item["name"],hex(x.reg_read(UC_X86_REG_EIP)),x.reg_read(UC_X86_REG_EAX),bytes(x.mem_read(base,28)).hex())
   assert bytes(x.mem_read(output,64))==want
  assert struct.unpack('<I',x.mem_read(base+16,4))[0]==len(data)
  levels+=1;total+=count;summary.append(dict(level=item['name'],regions=count))
# Every truncated prefix and malformed dimensions must preserve reader/output.
valid=w(1,123)+bytes(2)+struct.pack('<12f',0,0,0,1,0,0,0,1,0,0,0,1)+bytes(2)+bytes(1)+w(2)+struct.pack('<3f',1,2,3)
malformed=[valid[:n] for n in range(len(valid))]+[valid+b'X']
for value in (float('nan'),-1):
 malformed.append(valid[:-4]+struct.pack('<f',value))
for data in malformed:
 reader=w(base+0x8000,0xd00,0,len(data),4,1,0)
 x.mem_write(base+0x8000,w(base+0x9000));x.mem_write(base+0x8000+72,w(len(data)+8))
 x.mem_write(base,reader);x.mem_write(output,bytes([0xa5])*64)
 x.mem_write(stack,w(stop,base,output));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0
 assert bytes(x.mem_read(base,28))==reader and bytes(x.mem_read(output,64))==bytes([0xa5])*64
report=dict(result='PASS',levels=levels,regions=total,malformed_guards=len(malformed),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='Owned PC/NXDK region lifetimes, exact budget rejection/acceptance, allocation count and repeatable close verified. All installed v180 region sections independently parsed and compared with PC file reader and compiled NXDK next-record reader. NXDK raw file reads supplied. Does not execute original loader or connect live climb lifecycle.',results=summary)
(root/'artifacts/level-regions-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',levels,'levels',total,'regions, PC/NXDK')

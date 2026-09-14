"""Retained raw lightmap RGB against installed section1200 payloads."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text())
image_count=level_count=peak=0;sample=None
for level in levels:
 sections=[s for s in level['sections'] if s['type']=='0x1200']
 if not sections:continue
 section=sections[0];archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
 count=struct.unpack_from('<I',data)[0];at=4;images=[]
 for j in range(count):
  width,height=struct.unpack_from('<2I',data,at);at+=8;pixels=data[at:at+width*height*3];at+=len(pixels);images.append((width,height,pixels))
 assert at==len(data)
 rows=[list(map(int,line.split())) for line in subprocess.check_output([str(root/'build/pc/Release/rf_lightmap_probe.exe'),'--rgb',str(root/'Installed_Game'/level['archive']),level['file'],'16777216'],text=True).splitlines()]
 assert rows[0][0]==count and len(rows)==count+1;peak=max(peak,rows[0][1])
 for image,row in zip(images,rows[1:]):
  width,height,pixels=image;hash=2166136261
  for b in pixels:hash=((hash^b)*16777619)&0xffffffff
  assert row==[width,height,len(pixels),hash],(level['file'],row)
 if level['file']=='L1S1.rfl':sample=(data,images)
 level_count+=1;image_count+=count
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
B=0x30000000;H=0x31000000;STACK=B+0xe000;STOP=B+0xff00;u.mem_map(B,65536);u.mem_map(H,4*1024*1024)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
data,images=sample;cursor=H;live={}
def hook(u,a,size,context):
 global cursor
 sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];value=0
 if a==sym('rf_vpp_read'):
  _,_,offset,dest,count=struct.unpack('<5I',u.mem_read(sp+4,20));offset-=8;assert offset<=len(data) and count<=len(data)-offset
  if count:u.mem_write(dest,data[offset:offset+count])
 elif a==sym('free'):
  ptr=struct.unpack('<I',u.mem_read(sp+4,4))[0]
  if ptr:assert ptr in live;del live[ptr]
 else:
  count=struct.unpack('<I',u.mem_read(sp+4,4))[0]
  if a==sym('calloc'):count*=struct.unpack('<I',u.mem_read(sp+8,4))[0]
  value=cursor;cursor+=(count+15)&~15;assert cursor<=H+4*1024*1024;live[value]=count;u.mem_write(value,bytes(count))
 u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
for name in ('rf_vpp_read','malloc','calloc','free'):u.hook_add(UC_HOOK_CODE,hook,begin=sym(name),end=sym(name))
def call(name,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(sym(name),STOP,count=10000000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
u.mem_write(B,bytes(4096));u.mem_write(B,w(B+0x1000));u.mem_write(B+72,w(len(data)+8,180,0,1));u.mem_write(B+600,w(0x1200,0,len(data)));u.mem_write(B+0x2000,bytes(12))
budget=12+16*len(images)+sum(len(x[2]) for x in images)
assert call('rf_lightmap_rgb_open',[B+0x2000,B,budget-1])!=0 and not live and bytes(u.mem_read(B+0x2000,12))==bytes(12)
cursor=H;assert call('rf_lightmap_rgb_open',[B+0x2000,B,budget])==0
ptr,count,resident=struct.unpack('<3I',u.mem_read(B+0x2000,12));assert count==len(images) and resident==budget
for i,(width,height,pixels) in enumerate(images):
 address,iw,ih,size=struct.unpack('<4I',u.mem_read(ptr+i*16,16));assert (iw,ih,size)==(width,height,len(pixels)) and bytes(u.mem_read(address,size))==pixels
u.mem_write(B,bytes(4096));call('rf_lightmap_rgb_close',[B+0x2000]);call('rf_lightmap_rgb_close',[B+0x2000]);assert not live and bytes(u.mem_read(B+0x2000,12))==bytes(12)
report=dict(result='PASS',pc_levels=level_count,pc_images=image_count,peak_pc_owner_bytes=peak,nxdk_live_mines_images=len(images),nxdk_live_mines_owner_bytes=budget,scope='Exact original section RGB bytes, PC image hashes and compiled NXDK full Live Mines byte equality. Exact/short budgets, late allocation cleanup, source closure and repeated close. NXDK heap/archive hooks; no native whole-scene memory or live scheduler evidence.')
(root/'artifacts/lightmap-rgb-owner.json').write_text(json.dumps(report,indent=2));print(report)

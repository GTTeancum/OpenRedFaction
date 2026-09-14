"""Compiled NXDK rectangle conversion/swizzling and dirty-state guards."""
import json,random,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
B=0x30000000;STACK=B+0xf000;STOP=B+0xff00;u.mem_map(B,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
entry=int(re.search(r'\s_rf_lightmap_upload_image_1555\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def swizzle(x,y,width,height):
 bits=[]
 for b in range(max(width,height).bit_length()-1):
  if 1<<b<width:bits.append((x>>b)&1)
  if 1<<b<height:bits.append((y>>b)&1)
 return sum(v<<i for i,v in enumerate(bits))*2
rng=random.Random(0x4f2f0a);writes=guards=0
for case in range(1024):
 iw=1<<rng.randrange(7);ih=1<<rng.randrange(7);x=rng.randrange(iw);y=rng.randrange(ih);width=rng.randint(1,iw-x);height=rng.randint(1,ih-y)
 pitch=width*3+rng.randrange(5);data=rng.randbytes(pitch*height);initial=rng.randbytes(iw*ih*2);expected=bytearray(initial)
 dirty=(8,24,0,16,9)[case%5];empty=case%17==0;short=case%13==0;absent=case%19==0
 if empty:width=0
 size=len(data) if not short else max(0,(height-1)*pitch+width*3-1)
 error=not empty and bool(dirty&7)
 active=not empty and not error and bool(dirty&8) and not absent
 error=error or (active and size<(height-1)*pitch+width*3)
 final_dirty=dirty if empty or error else 0
 if active and not error:
  writes+=1
  for row in range(height):
   for col in range(width):
    r,g,b=data[row*pitch+col*3:row*pitch+col*3+3];value=0x8000|((r>>3)<<10)|((g>>3)<<5)|(b>>3)
    at=swizzle(x+col,y+row,iw,ih);expected[at:at+2]=struct.pack('<H',value)
 if error:guards+=1
 u.mem_write(B+0x1000,w(iw,ih,len(initial),5,0 if absent else B+0x4000));u.mem_write(B+0x2000,bytes([dirty]));u.mem_write(B+0x4000,initial);u.mem_write(B+0x8000,data)
 u.mem_write(STACK,w(STOP,B+0x1000,B+0x8000,size,pitch,x,y,width,height,B+0x2000));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(entry,STOP,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and bool(u.reg_read(UC_X86_REG_EAX))==error and u.mem_read(B+0x2000,1)[0]==final_dirty and bytes(u.mem_read(B+0x4000,len(initial)))==bytes(expected),case
for image_words,rectangle in (
 ((8,8,128,7,B+0x4000),(0,0,2,2)),
 ((7,8,112,5,B+0x4000),(0,0,2,2)),
 ((8,8,128,5,B+0x4000),(7,0,2,2)),
 ((8,8,128,5,B+0x4000),(0,7,2,2)),
):
 u.mem_write(B+0x1000,w(*image_words));u.mem_write(B+0x4000,b'\xa5'*128);u.mem_write(B+0x2000,b'\x08');u.mem_write(B+0x8000,bytes(12))
 u.mem_write(STACK,w(STOP,B+0x1000,B+0x8000,12,6,*rectangle,B+0x2000));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(entry,STOP,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_EAX)!=0 and bytes(u.mem_read(B+0x4000,128))==b'\xa5'*128 and u.mem_read(B+0x2000,1)[0]==8
 guards+=1
report=dict(result='PASS',nxdk_cases=1024,written_rectangles=writes,rejected_preserved_cases=guards,scope='Actual NXDK pixel addressing and conversion against independent Morton layout/1555 expectations, entire backing-image equality including untouched texels. Pitched local RGB, empty extents, unavailable pixels, dirty gates and short source. No native GPU synchronization/cache-invalidation evidence.')
(root/'artifacts/lightmap-image-upload.json').write_text(json.dumps(report,indent=2));print(report)

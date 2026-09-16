"""Original format selection, device allocation, real lock/unlock and crater RGB upload."""
from pathlib import Path
import hashlib
assert hashlib.sha256((Path(__file__).resolve().parents[1]/'Installed_Game/RF.exe').read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
exec((Path(__file__).parent /'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
D=B+0x1000;VT=B+0x2000;T=B+0x3000;TV=B+0x4000;REC=B+0x5000;TILES=B+0x6000;PIX=B+0x7000;M=B+0x8000;IMG=B+0x9000;RGB=B+0xa000;OUT=B+0xb000
create=B+0xc000;lock=create+16;desc=create+32;unlock=create+48;events=[];formats={25};width=height=1;pitch=2;selected=25

def boundary(cpu,a,size,_):
 if a not in [0x5463a0,0x50f440,create,lock,desc,unlock]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:rd(sp+4+i*4);pop=0;value=0
 if a==0x5463a0:value=int(arg(0) in formats)
 elif a==0x50f440:assert arg(0)==17;value=0
 elif a==create:
  args=[arg(i) for i in range(8)];assert args[:7]==[D,width,height,1,0,selected,1];u.mem_write(arg(7),w(T));events.append(dict(create=args[:7]));pop=32
 elif a==lock:
  assert [arg(i) for i in [0,1,3,4]]==[T,0,0,0];u.mem_write(arg(2),w(pitch,PIX));events.append('lock');pop=20
 elif a==desc:u.mem_write(arg(2),w(selected,3,0,1,pitch*height,0,width,height));events.append('description');pop=12
 elif a==unlock:assert [arg(0),arg(1)]==[T,0];events.append('unlock');pop=8
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,boundary)
u.mem_write(D,w(VT));u.mem_write(VT+0x50,w(create));u.mem_write(T,w(TV));u.mem_write(TV+0x40,w(lock));u.mem_write(TV+0x38,w(desc));u.mem_write(TV+0x44,w(unlock));u.mem_write(0x1cfcbe4,w(D));u.mem_write(0x1e65338,w(REC));u.mem_write(REC,w(17,1,0,TILES));u.mem_write(TILES,w(T));u.mem_write(0x17c7bcc,w(102));u.mem_write(M,bytes(124));u.mem_write(M+0x68,w(0xffffffff));u.mem_write(IMG,w(0,1,1,RGB,17,0));u.mem_write(0xc96890,b'\0');u.mem_write(0xc9b4b4,w(0))
def call(entry,args,ecx=0):
 u.mem_write(S,w(stop,*args));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_ECX,ecx);u.emu_start(entry,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
choices=[]
for formats in [{25,26,29},{26,29},{29},set()]:
 u.mem_write(0x1cfc5e4,w(0xffffffff));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x545e20,0x545e54,count=1000);actual=rd(0x1cfc5e4);assert actual==(min(formats) if formats else 0xffffffff);choices.append(dict(supported=sorted(formats),selected=actual))
rows=[];selected=25;u.mem_write(0x1cfc5e4,w(selected))
for width,height,padding in [(1,1,0),(2,1,4),(3,2,2),(8,4,8)]:
 pitch=width*2+padding;events.clear();call(0x55b970,[5,width,height,1,OUT]);assert rd(OUT)==T
 rgb=bytes((32+(i*13)%64) for i in range(width*height*3));u.mem_write(RGB,rgb);u.mem_write(PIX,b'\xa5'*(pitch*height));u.mem_write(IMG+4,w(width,height));u.mem_write(M+8,b'\x08');u.mem_write(M+12,w(IMG,0,0,width,height));call(0x4f26a0,[B+0xd000,0],M)
 expected=bytearray(b'\xa5'*(pitch*height))
 for y in range(height):
  for x in range(width):
   r,g,b=rgb[(y*width+x)*3:(y*width+x)*3+3];packed=0x8000|max(r>>3,4)<<10|max(g>>3,4)<<5|max(b>>3,4);struct.pack_into('<H',expected,y*pitch+x*2,packed)
 assert bytes(u.mem_read(PIX,len(expected)))==expected;assert bytes(u.mem_read(RGB,len(rgb)))==rgb;assert u.mem_read(M+8,1)[0]==0;assert struct.unpack('<H',u.mem_read(REC+8,2))[0]==0
 rows.append(dict(width=width,height=height,pitch=pitch,events=events[:],native_bytes=expected.hex(),base_unchanged=True))
(R/'artifacts/crater-shading-re/crater-device-upload.json').write_text(json.dumps(dict(format_choices=choices,uploads=rows),indent=2));print('PASS:4 format choices and4 native allocation/lock/upload/unlock lifecycles')


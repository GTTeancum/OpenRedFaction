"""Original zero-light direct ambient RGB fill vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VIEW=B+0x6000;OWNER=B+0x7000;IMAGE=B+0x7100;DIRTY=B+0x7200;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_fill_ambient\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)


ROOM_OWNER=B+0x8000;TABLE=B+0x8100;ROOM=B+0x9000;rng=random.Random(0x4f2719);inputs=[];responses=[]
for i in range(512):
 width=1+i%8;height=1+(i//8)%8;iw=width+2+i%3;ox=1;oy=i%2;pitch=iw*3;offset=(oy*iw+ox)*3;dirty=i%256
 global_rgb=[rng.choice([-2,-1,-.5,0,1,2,1/128,rng.uniform(-4,4)]) for j in range(3)];room=bytes([i%4]+[rng.randrange(256) for j in range(3)]);data=w(width,height,pitch,offset,dirty)+f(*global_rgb)+room;inputs.append(data)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+8,bytes([dirty]));o.mem_write(OWNER+12,w(IMAGE,ox,oy,width,height));o.mem_write(OWNER+104,w(0));o.mem_write(IMAGE,w(0,iw,16,OUT));o.mem_write(ROOM_OWNER+0x90,w(1,1,TABLE));o.mem_write(TABLE,w(ROOM));o.mem_write(ROOM+0x45,room);o.mem_write(0x5a38d4,f(*global_rgb));o.mem_write(OUT,bytes([165])*1024)
 o.mem_write(STACK,bytes(256));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EBX,ROOM_OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f2719,0x4f2c74,count=100000)
 assert o.reg_read(UC_X86_REG_EIP)==0x4f2c74
 expected=bytes(o.mem_read(OUT,1024));expected_dirty=bytes(o.mem_read(OWNER+8,1))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*1024);x.mem_write(DIRTY,bytes([dirty]));assert call([OUT+offset,1024-offset,pitch,width,height,B+20,B+32,DIRTY])==0
 assert bytes(x.mem_read(OUT,1024))==expected and bytes(x.mem_read(DIRTY,1))==expected_dirty,i;responses.append(w(0)+expected_dirty+expected)
probe=[str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-fill-ambient'];assert subprocess.check_output(probe,input=b''.join(inputs))==b''.join(responses)
for size,pitch,width,height in [(1,6,2,2),(1024,5,2,2),(1024,6,0,2)]:
 x.mem_write(OUT,bytes([165])*1024);x.mem_write(DIRTY,b'\x02');assert call([OUT,size,pitch,width,height,B+20,B+32,DIRTY])!=0 and bytes(x.mem_read(OUT,1024))==bytes([165])*1024 and bytes(x.mem_read(DIRTY,1))==b'\x02'
report=dict(result='PASS',original_pc_nxdk_rectangles=len(inputs),nxdk_guards=3,original_sha256=sha,x87_control_word='0x027f',scope='Original4f2719..4f2c74 direct ambient conversion/fill/dirty8 including actual room/global helpers and ftol; no hooks. Flags0..3, signed/wrapping colors, offsets/padding and every1..8 size pair. Light selection and subsequent upload excluded.')
(root/'artifacts/lightmap-fill-ambient.json').write_text(json.dumps(report,indent=2));print(report)

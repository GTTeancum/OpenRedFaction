"""Original4f26a0 RGB-only upload path vs PC/NXDK rectangle conversion."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;IMAGE=B+0x2000;RGB=B+0x3000;PACKED=B+0x4000;VIEW=B+0x5000;DIRTY=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_upload_rgb_1555\s+([0-9a-fA-F]+)',mp)[1],16)
locked=0;pitch=0;calls=[]
def hook(u,address,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);calls.append(address)
 if address==0x50e2e0:
  assert [read(u,sp+j) for j in (4,8,16)]==[7,0,1]
  out=read(u,sp+12);u.mem_write(out,bytes(28));u.mem_write(out+12,w(PACKED));u.mem_write(out+24,w(pitch));u.reg_write(UC_X86_REG_EAX,locked)
 u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4)
o.hook_add(UC_HOOK_CODE,hook,begin=0x50e2e0,end=0x50e2e0);o.hook_add(UC_HOOK_CODE,hook,begin=0x50e310,end=0x50e310)
def call():
 x.mem_write(STACK,w(STOP,VIEW,DIRTY));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f2f0a);inputs=[];responses=[];uploads=0
for i in range(2048):
 width=rng.randint(1,16);px=rng.randrange(width);py=rng.randrange(16);rw=rng.randint(0,width-px);rh=rng.randint(0,16-py);pitch=width*2+rng.randrange(9)*2;flags=rng.choice([0,8,128,136]);locked=i%5!=0
 rgb=bytes(rng.randrange(256) for _ in range(768)) if i%8 else bytes(768);packed=bytes([165])*768
 data=w(px,py,rw,rh,width,pitch,flags,int(locked))+rgb;inputs.append(data)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+8,bytes([flags]));o.mem_write(OWNER+12,w(IMAGE,px,py,rw,rh));o.mem_write(IMAGE,w(0,width,16,RGB,7,0));o.mem_write(RGB,rgb);o.mem_write(PACKED,packed)
 calls.clear();o.mem_write(STACK,w(STOP,0,0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.emu_start(0x4f26a0,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 expected=bytes(o.mem_read(OWNER+8,1))+bytes(o.mem_read(PACKED,768));assert bytes(o.mem_read(RGB,768))==rgb
 wanted=([0x50e2e0]+([0x50e310] if locked else [])) if rw and rh and flags&8 else [];assert calls==wanted
 uploads+=0x50e310 in calls
 x.mem_write(RGB,rgb);x.mem_write(PACKED,packed);x.mem_write(DIRTY,bytes([flags]));x.mem_write(VIEW,w(RGB,768,width*3,PACKED if locked else 0,768,pitch,px,py,rw,rh))
 assert call()==0;got=bytes(x.mem_read(DIRTY,1))+bytes(x.mem_read(PACKED,768));assert got==expected,(i,got.hex(),expected.hex());assert bytes(x.mem_read(RGB,768))==rgb
 responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-rgb-upload'],input=b''.join(inputs))==b''.join(responses)
# Nonempty dirty upload: invalid rectangle/pitch/buffers preserve destination and flags.
x.mem_write(VIEW,w(RGB,768,48,PACKED,768,32,0,0,16,16));x.mem_write(DIRTY,b'\x08')
for at,value in [(VIEW,0),(VIEW+4,767),(VIEW+8,47),(VIEW+16,511),(VIEW+20,31),(VIEW+24,0xffffffff),(VIEW+28,0xffffffff)]:
 old=bytes(x.mem_read(at,4));x.mem_write(at,w(value));before=bytes(x.mem_read(PACKED,768))+bytes(x.mem_read(DIRTY,1));assert call()!=0;assert bytes(x.mem_read(PACKED,768))+bytes(x.mem_read(DIRTY,1))==before;x.mem_write(at,old)
report=dict(result='PASS',original_pc_nxdk_cases=2048,successful_uploads=uploads,nxdk_guards=7,original_sha256=sha,scope='Complete original4f26a0 for RGB-only dirty states; only bitmap lock/unlock supplied. Rectangle offsets, byte pitch, dark/black texels, outside preservation, empty dimensions and failed locks. Preceding static/dynamic lighting and native renderer upload excluded.')
(root/'artifacts/lightmap-rgb-upload.json').write_text(json.dumps(report,indent=2));print(report)

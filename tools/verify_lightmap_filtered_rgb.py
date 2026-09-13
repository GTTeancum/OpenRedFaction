"""Unhooked original4f3100 neighborhood filtering vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x2000;VIEW=B+0x2100;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_filtered_rgb\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,address,args,cw=0x27f):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,cw);u.emu_start(address,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f3100);inputs=[];responses=[];border=0
for i in range(4096):
 width=rng.randint(2,16);height=rng.randint(2,16);px=rng.randrange(width);py=rng.randrange(height);border+=px in (0,width-1) or py in (0,height-1)
 channels=[]
 for c in range(3):
  if i%4==0:values=[(i%1024)/255]*256
  else:values=[rng.uniform(-2,4) if i%5 else rng.uniform(-100000,100000) for j in range(256)]
  channels.append(f(*values))
 data=w(width,height,px,py)+b''.join(channels);inputs.append(data)
 for c,a in enumerate([0x1431de0,0x14b23e0,0x13f1de0]):o.mem_write(a,channels[c])
 o.mem_write(OUT,bytes([165])*3);call(o,0x4f3100,[px,py,width,height,OUT]);expected=bytes(o.mem_read(OUT,3))
 x.mem_write(B,data);x.mem_write(VIEW,w(B+16,B+1040,B+2064,256,width,height));x.mem_write(OUT,bytes([165])*3)
 assert call(x,entry,[VIEW,px,py,OUT])==0;got=bytes(x.mem_read(OUT,3));assert got==expected,(i,width,height,px,py,got.hex(),expected.hex());responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-filtered-rgb'],input=b''.join(inputs))==b''.join(responses)
# Malformed views fail before output mutation.
for offset,value in [(0,0),(12,0),(16,1),(20,1),(16,0xffffffff)]:
 saved=bytes(x.mem_read(VIEW+offset,4));x.mem_write(VIEW+offset,w(value));x.mem_write(OUT,bytes([165])*3)
 assert call(x,entry,[VIEW,px,py,OUT])!=0 and bytes(x.mem_read(OUT,3))==bytes([165])*3;x.mem_write(VIEW+offset,saved)
report=dict(result='PASS',original_pc_nxdk_cases=4096,boundary_cases=border,nxdk_guards=5,x87_control_word='0x027f',original_sha256=sha,scope='Complete original4f3100 with unhooked CRT. Original53-bit x87 precision matching native replay; interior channel-specific sum order, boundary fallback, red/green float stores and blue retained precision. Caller selection, source accumulation and renderer binding excluded.')
(root/'artifacts/lightmap-filtered-rgb.json').write_text(json.dumps(report,indent=2));print(report)

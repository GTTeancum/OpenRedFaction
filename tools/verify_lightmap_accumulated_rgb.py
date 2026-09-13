"""Unhooked original4f3040 accumulated-light RGB conversion vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x1000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_accumulated_rgb\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,address,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(address,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f3040);inputs=[];responses=[];normalizations=0;wrapped=0
for i in range(8192):
 if i<4096:
  n=i%1024;bits=struct.unpack('<I',f(n/255))[0];near=struct.unpack('<f',w(bits+(i%3-1) if bits else 0))[0];v=[near,near*.51,-near if i%2 else near*1.2]
 else:v=[rng.uniform(-2,8) if i%3 else rng.uniform(-8000000,8000000) for _ in range(3)]
 data=f(*v);values=struct.unpack('<3f',data);ints=[max(0,int(v*255)) for v in values];normalizations+=max(ints)>255;wrapped+=max(ints)*255>0x7fffffff
 inputs.append(data)
 for j,a in enumerate([0x1431de0,0x14b23e0,0x13f1de0]):o.mem_write(a,data[j*4:j*4+4])
 o.mem_write(OUT,bytes([165])*3);call(o,0x4f3040,[0,OUT]);expected=bytes(o.mem_read(OUT,3))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*3);assert call(x,entry,[B,OUT])==0;assert bytes(x.mem_read(OUT,3))==expected,(i,values,expected.hex(),bytes(x.mem_read(OUT,3)).hex());responses.append(w(0)+expected)
 # In-place output must not corrupt unread float channels.
 x.mem_write(B,data);assert call(x,entry,[B,B])==0;assert bytes(x.mem_read(B,3))==expected
for v in [float('nan'),float('inf'),float('-inf'),9000000,-9000000]:
 data=f(.5,v,1);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,bytes([165])*3);status=call(x,entry,[B,OUT]);assert status and bytes(x.mem_read(OUT,3))==bytes([165])*3;responses.append(w(status)+bytes([165])*3)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-accumulated-rgb'],input=b''.join(inputs))==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=8192,normalized_cases=normalizations,wrapped_numerator_cases=wrapped,in_place_nxdk_cases=8192,invalid_pc_nxdk_cases=5,original_sha256=sha,scope='Complete original4f3040 and CRT conversion unhooked; three accumulated float channels, truncation boundaries, negative clamp, integer peak normalization and wrapped signed numerator. Light accumulation/filtering and native rendering excluded.')
(root/'artifacts/lightmap-accumulated-rgb.json').write_text(json.dumps(report,indent=2));print(report)

"""Replay original4f25a0 shadow polygon area against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_area\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f25a0);inputs=[];responses=[];zero=positive=late_zero=0
o.mem_write(STOP,b'\xd9\x1d'+w(OUT))
for i in range(4096):
 n=i%33;vertices=[[rng.uniform(-64,64),rng.uniform(-64,64)] for _ in range(n)]
 if i%7==0:vertices=[[j,2*j] for j in range(n)]
 if i%7==1 and n>=4:vertices[0:4]=[[0,0],[4,0],[4,4],[4,4]]
 if i%7==2:vertices=[[j,j+rng.uniform(-1e-6,1e-6)] for j in range(n)]
 data=w(n)+b''.join(f(*v) for v in vertices).ljust(256,b'\0');o.mem_write(B,data);call(o,0x4f25a0,[n,B+4]);o.emu_start(STOP,STOP+6,count=1);expected=bytes(o.mem_read(OUT,4))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*4);assert call(x,entry,[B+4,n,OUT])==0
 got=bytes(x.mem_read(OUT,4));assert got==expected,(i,got.hex(),expected.hex());inputs.append(data);responses.append(w(0)+expected)
 value=struct.unpack('<f',expected)[0];assert value>=0
 if value==0:zero+=1
 else:positive+=1
 if i%7==1 and n>=4:assert value==0;late_zero+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-area'],input=b''.join(inputs))==b''.join(responses)
for bad in [w(0xffffffff)+data[4:],w(3)+f(float('nan'))+data[8:],w(3)+f(float('inf'))+data[8:]]:
 x.mem_write(B,bad);x.mem_write(OUT,bytes([165])*4);nn=struct.unpack('<I',bad[:4])[0];status=call(x,entry,[B+4,nn,OUT]);assert status!=0 and bytes(x.mem_read(OUT,4))==bytes([165])*4
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-area'],input=bad)==w(status)+bytes([165])*4
report=dict(result='PASS',original_pc_nxdk_polygons=len(inputs),positive=positive,zero=zero,late_degenerate_zero=late_zero,pc_nxdk_guards=3,original_sha256=sha,x87_control_word='0x027f',scope='Full unhooked4f25a0 with actual vector subtraction/length helpers. Original returned x87 value stored as binary32; fan order, mixed side-length precision, multiplication order, accumulation, early and late degeneration exact. Polygon clipping/filter threshold and native mask integration excluded.')
(root/'artifacts/lightmap-shadow-area.json').write_text(json.dumps(report,indent=2));print(report)

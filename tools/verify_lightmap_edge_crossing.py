"""Replay full original508f70 segment crossing against shared PC/NXDK code."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_edge_crossing\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x508f70);cases=[]
for scale in [1,2**-80,2**-60,2**-20,2**20,2**60,2**80]:
 for v in [(0,0,1,1,0,1,1,0),(0,0,1,0,1,0,1,1),(0,0,1,0,0,1,1,1),(0,0,1,0,.5,0,2,0),(0,0,0,0,0,0,1,1),(0,0,1,1,0,2**-23,1,1+2**-23)]:cases.append(f(*(a*scale for a in v)))
for i in range(8192):
 scale=2**rng.randint(-80,80);v=[rng.uniform(-2,2)*scale for _ in range(8)]
 if i%4==0:v[4:6]=v[2:4]
 cases.append(f(*v))
responses=[];hits=0
for i,data in enumerate(cases):
 o.mem_write(B,data);expected=call(o,0x508f70,[B,B+8,*struct.unpack('<4I',data[16:])])&255;assert expected in (0,1)
 x.mem_write(B,data);x.mem_write(OUT,w(0xa5a5a5a5));assert call(x,entry,[B,B+8,B+16,B+24,OUT])==0
 got=struct.unpack('<I',x.mem_read(OUT,4))[0];assert got==expected,(i,struct.unpack('<8f',data),got,expected)
 hits+=expected;responses.append(w(0,expected))
probe=[str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-edge-crossing']
assert subprocess.check_output(probe,input=b''.join(cases))==b''.join(responses)
guards=[f(math.nan,0,1,1,0,1,1,0),f(0,0,math.inf,1,0,1,1,0),f(-3.4e38,0,3.4e38,1,0,1,1,0)]
for data in guards:
 x.mem_write(B,data);x.mem_write(OUT,w(0xa5a5a5a5));status=call(x,entry,[B,B+8,B+16,B+24,OUT]);assert status!=0 and bytes(x.mem_read(OUT,4))==w(0xa5a5a5a5)
 assert subprocess.check_output(probe,input=data)==w(status,0xa5a5a5a5)
report=dict(result='PASS',original_pc_nxdk_cases=len(cases),crossings=hits,guards=len(guards),original_sha256=sha,x87_control_word='0x027f',scope='Full original508f70, no hooks; inclusive endpoints, parallel/collinear rejection and retained/stored determinant precision over tiny/large ranges. Special polygon coverage orchestration and rendered lighting excluded.')
(root/'artifacts/lightmap-edge-crossing.json').write_text(json.dumps(report,indent=2));print(report)

"""Replay full original54a1c0 polygon/plane clipping against PC/NXDK."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_clip_shadow\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x54a1c0);inputs=[];responses=[];emitted=0;crossings=0;empty=0
for i in range(2048):
 n=2+i%31;plane=[rng.uniform(-2,2) for _ in range(4)];vertices=[[rng.uniform(-20,20) for _ in range(3)] for _ in range(n)]
 if i%4==0:
  plane=[1,0,0,0];values=[0,.000099,.0001,.000101,-.0001,-1,1]
  for j in range(n):vertices[j][0]=values[(i+j)%len(values)]
 if i%19==0:plane=[0,0,0,1]
 if i%23==0:plane=[0,0,0,-1]
 if i==0:n=0;vertices=[]
 raw=b''.join(f(*v) for v in vertices).ljust(384,b'\0');data=w(n,64)+f(*plane)+raw
 o.mem_write(B,data);o.mem_write(OUT,bytes([165])*768);expected_n=call(o,0x54a1c0,[n,B+24,OUT,B+8]);assert expected_n<=2*n
 expected=bytes(o.mem_read(OUT,768));x.mem_write(B,data);x.mem_write(OUT,bytes([165])*768);x.mem_write(OWNER,w(0xa5a5a5a5))
 assert call(x,entry,[B+24,n,B+8,OUT,64,OWNER])==0
 got_n=struct.unpack('<I',x.mem_read(OWNER,4))[0];got=bytes(x.mem_read(OUT,768));assert got_n==expected_n and got==expected,(i,got_n,expected_n,[(j,a,b) for j,(a,b) in enumerate(zip(got,expected)) if a!=b][:12])
 inputs.append(data);responses.append(w(0,expected_n)+expected);emitted+=expected_n;empty+=expected_n==0
 original_vertices={raw[j*12:j*12+12] for j in range(n)};crossings+=sum(expected[j*12:j*12+12] not in original_vertices for j in range(expected_n))
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-clip-shadow'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,w(1)),(4,w(2*n-1)),(8,f(float('nan'))),(24,f(float('inf')))]:
 bad=bytearray(data);bad[at:at+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*768);x.mem_write(OWNER,w(0xa5a5a5a5));nn,cap=struct.unpack('<II',bad[:8])
 status=call(x,entry,[B+24,nn,B+8,OUT,cap,OWNER]);assert status!=0 and bytes(x.mem_read(OUT,768))==bytes([165])*768 and bytes(x.mem_read(OWNER,4))==w(0xa5a5a5a5)
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-clip-shadow'],input=bytes(bad))==w(status,0xa5a5a5a5)+bytes([165])*768
report=dict(result='PASS',original_pc_nxdk_polygons=len(inputs),emitted_vertices=emitted,intersection_vertices=crossings,empty_polygons=empty,pc_nxdk_guards=4,original_sha256=sha,x87_control_word='0x027f',scope='Full unhooked54a1c0 with actual54a320 plane classification and vector helpers; arbitrary ordered polygons, epsilon boundaries, all in/out, zero normals and empty input. Output order/float bits and untouched tail exact. Compiled NXDK CPU replay; six-plane volume construction and native rendered shadows excluded.')
(root/'artifacts/lightmap-clip-shadow.json').write_text(json.dumps(report,indent=2));print(report)

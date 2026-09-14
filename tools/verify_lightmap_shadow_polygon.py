"""Replay original4f4590 shadow ray/projection/deduplication loop against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_polygon\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
def reject(u,address,size,context):u.emu_stop()
o.hook_add(UC_HOOK_CODE,reject,begin=0x4f5515,end=0x4f5515)
rng=random.Random(0x4f50c9);inputs=[];responses=[];accepted=rejected=removed=0
for i in range(1024):
 n=3+i%30;normal=i%3;uaxis=[j for j in range(3) if j!=normal][(i//3)%2];vaxis=3-normal-uaxis;width=2+i%63;height=2+(i//63)%63;iw=128;ih=128;ox=8;oy=8
 scale=[.01,.01];offset=[.1,.1];origin=[0,0,0];plane=[0,0,0,-10];plane[normal]=1
 vertices=[[rng.uniform(-30,30) for _ in range(3)] for _ in range(n)]
 for v in vertices:v[normal]=rng.uniform(1,9)
 if i%4==0:vertices[-1]=vertices[0][:]
 if i%5==0:vertices[1]=vertices[0][:]
 if i%7==0:vertices[i%n][normal]=-1
 if i%11==0:vertices[i%n][normal]=0
 view=w(iw,ih,ox,oy)+f(*scale,*offset,*plane)+w(normal,uaxis)
 raw=b''.join(f(*v) for v in vertices).ljust(384,b'\0');data=view+w(width,height,n,32)+f(*origin,*plane)+raw
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(OWNER+256,ox,oy,width,height));o.mem_write(OWNER+256,w(0,iw,ih));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis,vaxis))
 o.mem_write(B+0x9000,raw);o.mem_write(STACK,bytes(4096));o.mem_write(STACK+0x64,f(*origin));o.mem_write(STACK+0x1b0,f(*plane));o.mem_write(STACK+0x2f0,bytes([165])*512);o.mem_write(B+0xf000,bytes(32));o.mem_write(B+0xf00c,w(OWNER))
 o.reg_write(UC_X86_REG_EBP,B+0xf000);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EDI,n);o.reg_write(UC_X86_REG_EBX,B+0x9000);o.reg_write(UC_X86_REG_FPCW,0x27f)
 o.emu_start(0x4f50c9,0x4f53bd,count=1000000);end=o.reg_read(UC_X86_REG_EIP);assert end in (0x4f53bd,0x4f5515)
 expected_n=o.reg_read(UC_X86_REG_ESI) if end==0x4f53bd else 0;expected=bytes(o.mem_read(STACK+0x2f0,256))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*256);x.mem_write(OWNER,w(0xa5a5a5a5));assert call(x,entry,[B,width,height,B+72,B+84,B+100,n,OUT,32,OWNER])==0
 got_n=struct.unpack('<I',x.mem_read(OWNER,4))[0];got=bytes(x.mem_read(OUT,256));assert got_n==expected_n and got==expected,(i,end,got_n,expected_n,got.hex(),expected.hex())
 inputs.append(data);responses.append(w(0,expected_n)+expected)
 if end==0x4f5515:rejected+=1
 else:accepted+=1;removed+=n-expected_n
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-polygon'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(64,w(2)),(68,w(n-1)),(72,f(float('nan'))),(100,f(float('inf')))]:
 bad=bytearray(data);bad[at:at+4]=value;nn,cap=struct.unpack('<II',bad[64:72]);x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*256);x.mem_write(OWNER,w(0xa5a5a5a5))
 status=call(x,entry,[B,width,height,B+72,B+84,B+100,nn,OUT,cap,OWNER]);assert status!=0 and bytes(x.mem_read(OUT,256))==bytes([165])*256 and bytes(x.mem_read(OWNER,4))==w(0xa5a5a5a5)
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-polygon'],input=bytes(bad))==w(status,0xa5a5a5a5)+bytes([165])*256
report=dict(result='PASS',original_pc_nxdk_polygons=len(inputs),accepted=accepted,rejected=rejected,duplicate_vertices_removed=removed,pc_nxdk_guards=4,original_sha256=sha,x87_control_word='0x027f',scope='Original4f50c9..4f53bd complete ray construction, actual5085c0, coordinate projection/clamping, consecutive/closing duplicate removal; rejection stop at4f5515. Six axis orders, parallel/behind rays, partial scratch exact. Clipped vertices/receiver plane supplied; volume construction, polygon area/filtering, mask ownership and native visuals excluded.')
(root/'artifacts/lightmap-shadow-polygon.json').write_text(json.dumps(report,indent=2));print(report)

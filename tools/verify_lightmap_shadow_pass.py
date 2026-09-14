"""Replay full original shadow receiver filtering and mask filling against PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EDI
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_lightmap_shadow_pass_polygon\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
calls=[];projected_snapshots=[]
def observe(u,a,size,ctx):calls.append(a)
def snapshot(u,a,size,ctx):projected_snapshots.append(u.reg_read(UC_X86_REG_ESI))
o.hook_add(UC_HOOK_CODE,observe,begin=0x4f2100,end=0x4f2100)
o.hook_add(UC_HOOK_CODE,snapshot,begin=0x4f53bd,end=0x4f53bd)
rng=random.Random(0x4f5069);inputs=[];responses=[];projected_total=accepted_total=changed=degenerate=0
for i in range(512):
 n=3+i%6;width=16;height=16;iw=128;ih=128;ox=8;oy=8;scale=[.01,.01];offset=[.1,.1];origin=[0,0,0]
 planes=[[0,0,-1,1],[0,0,1,-10],[1,0,0,-2],[-1,0,0,-2],[0,1,0,-2],[0,-1,0,-2]]
 cx=rng.uniform(-5,5);cy=rng.uniform(-5,5);z=rng.uniform(.5,11);radius=rng.uniform(.1,5)
 vertices=[(cx+radius*math.cos(j*2*math.pi/n),cy+radius*math.sin(j*2*math.pi/n),z) for j in range(n)]
 view=w(iw,ih,ox,oy)+f(*scale,*offset,0,0,1,-10)+w(2,0);rawvertices=b''.join(f(*v) for v in vertices).ljust(96,b'\0')
 receiver=f(-20,-20,40,-20,40,40,-20,40)+bytes(32);threshold=[rng.choice([0,.1,2,100]),1];amount=[127,255][i%2];capacity=width*(height+1)+1;initial=rng.randbytes(1024)
 data=view+w(width,height)+f(*origin)+b''.join(f(*p) for p in planes)+w(n,amount,capacity)+rawvertices+receiver+w(4)+f(*threshold)+initial
 assert len(data)==1380
 o.mem_write(STACK,bytes(4096));o.mem_write(STACK+0x64,f(*origin));o.mem_write(STACK+0x1a0,b''.join(f(*p) for p in planes));o.mem_write(STACK+0x40,w(1,1,B+0x1800));o.mem_write(STACK+0x70,w(amount))
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(OWNER+256,ox,oy,width,height));o.mem_write(OWNER+256,w(0,iw,ih));o.mem_write(OWNER+0x2c,f(*threshold));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(2,0,1));o.mem_write(B+0xf000,bytes(32));o.mem_write(B+0xf00c,w(OWNER));o.mem_write(B+0xf018,w(OUT));o.mem_write(OUT,initial)
 o.mem_write(B+0x1000,receiver+bytes(192)+w(4));o.mem_write(B+0x1800,w(B+0x1000));o.mem_write(0x1471de0,rawvertices)
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,B+0xf000);o.reg_write(UC_X86_REG_EDI,n);o.reg_write(UC_X86_REG_FPCW,0x27f);calls.clear();projected_snapshots.clear();o.emu_start(0x4f5069,0x4f5515,count=1000000)
 assert o.reg_read(UC_X86_REG_EIP)==0x4f5515
 expected=bytes(o.mem_read(OUT,1024));projected=o.mem_read(STACK+0x23,1)[0];accepted=int(bool(calls));assert projected in (0,1) and len(calls)<=1
 x.mem_write(B+0x3800,bytes(512));x.mem_write(B,data);x.mem_write(B+0x2000,data[:172]+w(B+0x6100));x.mem_write(B+0x6000,w(B+280,4));x.mem_write(B+0x6100,w(B+0x6000,1)+f(*threshold)+w(B+0x6200,B+0xb000,64));x.mem_write(B+0x6200,w(B+0x8000,B+0x9000,B+0xa000,64));x.mem_write(B+0xb000,bytes(512));x.mem_write(B+0x6300,w(B+0x3000,B+0x3400,B+0x3800,64));x.mem_write(OUT,w(0xffffffff,0xffffffff))
 x.mem_write(STACK,w(STOP,B+0x2000,B+184,n,B+0x6300,B+356,capacity,amount,OUT,OUT+4));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000)
 status=x.reg_read(UC_X86_REG_EAX);
 assert x.reg_read(UC_X86_REG_EIP)==STOP and status==0 and bytes(x.mem_read(OUT,8))==w(projected,accepted) and bytes(x.mem_read(B+356,1024))==expected,(i,status,projected,accepted,bytes(x.mem_read(OUT,8)).hex())
 degenerate+=int(bool(projected_snapshots) and projected_snapshots[0]<3);inputs.append(data);responses.append(w(0,projected,accepted)+expected);projected_total+=projected;accepted_total+=accepted;changed+=sum(a!=b for a,b in zip(initial,expected))
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-pass'],input=b''.join(inputs))==b''.join(responses)
for offset,value in ((172,2),(180,1)):
 bad=bytearray(data);bad[offset:offset+4]=w(value);nn,am,bs=struct.unpack_from('<3I',bad,172)
 x.mem_write(B,bytes(bad));x.mem_write(OUT,w(0xffffffff,0xffffffff));x.mem_write(STACK,w(STOP,B+0x2000,B+184,nn,B+0x6300,B+356,bs,am,OUT,OUT+4));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000);status=x.reg_read(UC_X86_REG_EAX)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and status!=0 and bytes(x.mem_read(OUT,8))==w(0xffffffff,0xffffffff) and bytes(x.mem_read(B+356,1024))==initial
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-pass'],input=bytes(bad))==w(status,0xffffffff,0xffffffff)+initial
x.mem_write(B,data);x.mem_write(B+0x630c,w(1));x.mem_write(OUT,w(0xffffffff,0xffffffff));x.mem_write(STACK,w(STOP,B+0x2000,B+184,n,B+0x6300,B+356,capacity,amount,OUT,OUT+4));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(OUT,8))==w(0xffffffff,0xffffffff) and bytes(x.mem_read(B+356,1024))==initial
report=dict(result='PASS',original_pc_nxdk_passes=len(inputs),projected=projected_total,degenerate_projections=degenerate,shared_guards=2,nxdk_capacity_guards=1,raster_accepted=accepted_total,changed_mask_bytes=changed,original_sha256=sha,scope='Unhooked4f5069..4f5515 with projection/raster observation only; all six clipping planes, ray/projection/deduplication, receiver clipping/area and mask subtraction combined. Explicit zero-initialized retained UV/filter scratch per case. Source/mapping preparation, face selection/traversal, border and native rendering remain external.')
(root/'artifacts/lightmap-shadow-pass.json').write_text(json.dumps(report,indent=2));print(report)

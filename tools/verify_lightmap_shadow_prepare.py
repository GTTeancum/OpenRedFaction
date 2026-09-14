"""Verify combined shadow cull/pass preparation against original chunks and PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_prepare\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
def stop_reject(u,a,size,ctx):u.emu_stop()
o.hook_add(UC_HOOK_CODE,stop_reject,begin=0x4f561a,end=0x4f561a)
phase='facing'
def stop_facing(u,a,size,ctx):
 if phase=='facing':u.emu_stop()
o.hook_add(UC_HOOK_CODE,stop_facing,begin=0x4f4b9e,end=0x4f4b9e)
rng=random.Random(0x4f4637);inputs=[];responses=[];facing_count=0
for i in range(1024):
 width=3+i%62;height=3+(i//62)%62;iw=width+rng.randrange(256);ih=height+rng.randrange(256);ox=rng.randrange(iw-width+1);oy=rng.randrange(ih-height+1)
 normal=i%3;uaxis=[j for j in range(3) if j!=normal][(i//3)%2];scale=[rng.choice([-1,1])*rng.uniform(.01,2) for _ in range(2)];offset=[rng.uniform(-1,1) for _ in range(2)];plane=[rng.uniform(-2,2) for _ in range(4)];plane[normal]=rng.choice([-1,1])*rng.uniform(.1,2);origin=[rng.uniform(-100,100) for _ in range(3)]
 view=w(iw,ih,ox,oy)+f(*scale,*offset,*plane)+w(normal,uaxis)
 minimum=[rng.uniform(-20,0) for _ in range(3)];maximum=[rng.uniform(0,20) for _ in range(3)];center=[rng.uniform(-20,20) for _ in range(3)];radius=rng.uniform(0,100)
 # Round fixture values before constructing expectations, matching stored input.
 minimum=list(struct.unpack('<3f',f(*minimum)));maximum=list(struct.unpack('<3f',f(*maximum)));center=list(struct.unpack('<3f',f(*center)));radius=struct.unpack('<f',f(radius))[0];origin=list(struct.unpack('<3f',f(*origin)))
 mapping=w(0,ox,oy,width,height)+f(1,1,*minimum,*maximum,*plane)+w(0,0,normal,uaxis,3-normal-uaxis)+f(*scale,*offset)+w(0xffffffff)
 data=mapping+view+w(i)+f(*center,radius,*origin);assert len(data)==196
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(OWNER+256,ox,oy,width,height));o.mem_write(OWNER+256,w(0,iw,ih));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis,3-normal-uaxis));o.mem_write(OWNER+0x6c,f(*plane))
 o.mem_write(OWNER+0x34,f(*minimum,*maximum));o.mem_write(STACK,bytes(4096));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f4637,0x4f4738,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f4738
 positions=bytes(o.mem_read(STACK+0x8c,48))+bytes(o.mem_read(STACK+0xc8,12))
 o.mem_write(B+0x9000,f(*origin));o.mem_write(STACK+0x3c,w(B+0x9000));o.mem_write(B+0xf00c,w(OWNER));o.reg_write(UC_X86_REG_EBP,B+0xf000);phase="facing";o.emu_start(0x4f4b32,0x4f4b9e,count=1000000);end=o.reg_read(UC_X86_REG_EIP);assert end in (0x4f4b9e,0x4f561a);facing=int(end==0x4f4b9e);expected=positions+w(facing)
 if facing:
  phase="volume";o.emu_start(0x4f4b9e,0x4f4daa,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f4daa
  planes=bytes(o.mem_read(STACK+0x1a0,96));bounds=bytes(o.mem_read(STACK+0xd4,12))+bytes(o.mem_read(STACK+0xf8,12))
  cull=f(*[v-radius for v in center],*[v+radius for v in center])+bounds+f(*plane)+planes+w(i)
  packed_pass=view+w(width,height)+f(*origin)+planes
 else:cull=bytes([165])*164;packed_pass=bytes([165])*172
 expected=cull+packed_pass
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*340)
 assert call(x,entry,[B,B+108,i,B+168,struct.unpack('<I',f(radius))[0],B+184,B+0x6000,OUT,OUT+164,OUT+336])==0
 got=bytes(x.mem_read(OUT,336));assert got==expected and bytes(x.mem_read(OUT+336,4))==w(facing),(i,got.hex(),expected.hex())
 inputs.append(data);responses.append(w(0,facing)+expected);facing_count+=facing
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-prepare'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(180,f(-1)),(168,f(float('nan'))),(164,w(0xffffffff)),(108,w(0))]:
 bad=bytearray(data);bad[at:at+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*340);idx,rad=struct.unpack_from('<I',bad,164)[0],struct.unpack_from('<I',bad,180)[0]
 status=call(x,entry,[B,B+108,idx,B+168,rad,B+184,B+0x6000,OUT,OUT+164,OUT+336]);assert status!=0 and bytes(x.mem_read(OUT,340))==bytes([165])*340
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-prepare'],input=bytes(bad))==w(status,0xffffffff)+bytes([165])*336
report=dict(result='PASS',original_pc_nxdk_preparations=len(inputs),facing=facing_count,rejected=len(inputs)-facing_count,guards=4,original_sha256=sha,scope='Extents3..64; original corner chunk and continuous facing/bounds/volume4f4b32..4f4daa; source center/radius bounds independently computed from serialized floats. Complete cull/pass bytes except borrowed filter pointer. Two-texel degenerate volumes remain unresolved. No native source traversal or rendered masks yet.')
(root/'artifacts/lightmap-shadow-prepare.json').write_text(json.dumps(report,indent=2));print(report)

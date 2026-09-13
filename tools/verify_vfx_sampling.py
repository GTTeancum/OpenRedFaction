"""Original frame selection and signed quantized vertex expansion."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;FACE=B+0x2000;UV=B+0x3000;PTR=B+0x4000;CTX=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe');stream=dict(data=b'',at=0)
lookup_names=[]
def file_service(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);pop=0;result=0
 if a==0x52cf60:
  target=read(u,sp+4);amount=read(u,sp+8);assert read(u,sp+12)==read(u,sp+16)==0
  chunk=stream['data'][stream['at']:stream['at']+amount];assert len(chunk)==amount,(hex(a),stream['at'],amount,len(stream['data']))
  u.mem_write(target,chunk);stream['at']+=amount;pop=16
 elif a==0x50f6a0:
  pointer=read(u,sp+4);name=bytes(u.mem_read(pointer,33)).split(b'\0')[0];lookup_names.append(name);result=0xffffffff
 else:assert a==0x524530
 u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
for a in (0x52cf60,0x524530,0x50f6a0):o.hook_add(UC_HOOK_CODE,file_service,begin=a,end=a)
PARAM=B+0x7000;COUNTS=B+0x8000;BLEND=B+0x9000;COLOR=B+0xa000;ALPHA=B+0xb000;SAMPLE=B+0xc000
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

rng=random.Random(0x53f060);probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe');inputs=[];responses=[]
def inactive(u,a,size,data):u.emu_stop()
o.hook_add(UC_HOOK_CODE,inactive,begin=0x54043d,end=0x54043d)
for i in range(2048):
 rate=rng.randrange(1,120);n=rng.randrange(1,200);start=rng.uniform(-2,2);time=rng.uniform(-50,100);flags=i%4
 if i<12:rate=15;n=4;start=0;time=[-1,0,.25,1,2,3,3.5,4,4.01,5,0,2][i]
 cfg=w(rate<<2,n)+f(start,0)+w(0,flags)+f(time);inputs.append(cfg)
 x.mem_write(B,cfg);x.mem_write(OUT,b'\xa5'*20);status=call('rf_vfx_frame_select',[B,flags,struct.unpack_from('<I',cfg,24)[0],OUT]);assert status==0;got=w(status)+bytes(x.mem_read(OUT,20));responses.append(got)
 o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x88,w(rate<<2));o.mem_write(OWNER+0xa4,w(n)+cfg[8:12]);o.mem_write(OWNER+0x114,w(flags));o.mem_write(FACE,w(OWNER));o.mem_write(STACK-0x200,bytes(0x200));o.mem_write(STACK,w(STOP)+cfg[24:28]);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,FACE);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53f060,0x53f127,count=1000000);active=o.reg_read(UC_X86_REG_EIP)==0x53f127;sp=o.reg_read(UC_X86_REG_ESP);position=bytes(o.mem_read(sp+0x38,4))
 if active:
  first=read(o,sp+0x40);second=o.reg_read(UC_X86_REG_EBP);expected=position+bytes(o.mem_read(sp+0x28,4))+w(min(first,n-1),min(second,n-1),1)
 else:assert o.reg_read(UC_X86_REG_EIP)==0x54043d;expected=position+bytes(16)
 assert got[4:]==expected,(i,cfg.hex(),got.hex(),expected.hex())
for cfg in [w(60,0)+f(0,0)+w(0,0)+f(0),w(60,1)+f(0,0)+w(0,0,0x7fc00000)]:
 inputs.append(cfg);x.mem_write(B,cfg);x.mem_write(OUT,b'\xa5'*20);status=call('rf_vfx_frame_select',[B,0,struct.unpack_from('<I',cfg,24)[0],OUT]);assert status!=0 and bytes(x.mem_read(OUT,20))==b'\xa5'*20;responses.append(w(status)+bytes(x.mem_read(OUT,20)))
assert subprocess.check_output([probe,'--vfx-frame-select'],input=b''.join(inputs))==b''.join(responses)
vertices=[];results=[]
for i in range(2048):
 data=struct.pack('<3h',*[rng.randrange(-32768,32768) for _ in range(3)])+f(*[rng.uniform(-10,10) for _ in range(6)]);vertices.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12)
 assert call('rf_vfx_vertex_decode',[B,6,B+6,OUT])==0;got=bytes(x.mem_read(OUT,12));results.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(STACK,w(STOP,OUT,B)+data[6:]);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x53cca0,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert got==bytes(o.mem_read(OUT,12)),(i,got.hex(),bytes(o.mem_read(OUT,12)).hex())
assert subprocess.check_output([probe,'--vfx-vertex'],input=b''.join(vertices))==b''.join(results)
report=dict(result='PASS',frame_selection_original_pc_nxdk=2048,vertex_original_pc_nxdk=2048,guards=2,scope='Original53f060..53f127/inactive exit and complete53cca0 with actual math helpers; no hooks except inactive stop. Safe terminal indices normalized. No mesh interpolation/pose/rendering or native XEMU.')
(root/'artifacts/vfx-sampling.json').write_text(json.dumps(report,indent=2));print(report)

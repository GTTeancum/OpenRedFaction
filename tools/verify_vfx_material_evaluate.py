"""Borrowed VFX material track evaluation against original scalar routines."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x54ab20);inputs=[];responses=[];TRAMP=B+0xf000
for i in range(2048):
 count=rng.randrange(1,33);rate=rng.choice([0,1,15,30,60,1000]);track=i%3;at=rng.randrange(1,8);time=f(rng.random()*count*20/max(1,rate));samples=f(*[rng.uniform(-1,2) for _ in range(count)])
 data=bytes(at)+samples;view=[0]*52;view[0]=1;view[30]=rate;view[[31,46,48][track]]=count;view[[32,47,49][track]]=at;view[50]=len(data);view=w(*view)
 inputs.append(w(len(data),track)+time+view+data);x.mem_write(B,data);x.mem_write(OWNER,view);x.mem_write(OUT,b'\xa5'*4)
 assert call('rf_vfx_material_evaluate',[B,len(data),OWNER,track,struct.unpack('<I',time)[0],OUT])==0;got=bytes(x.mem_read(OUT,4));responses.append(w(0)+got)
 original_samples=f(*[min(1,max(0,v)) for v in struct.unpack('<'+'f'*count,samples)]) if track==0 else samples
 o.mem_write(B,original_samples);o.mem_write(OWNER,bytes(200));o.mem_write(OWNER,w(1));o.mem_write(OWNER+0x78,w(rate));o.mem_write(OWNER+[0x7c,0xb8,0xc0][track],w(count,B));o.mem_write(TRAMP,b'\xd9\x1d'+w(OUT)+b'\x68'+w(STOP)+b'\xc3')
 o.mem_write(STACK,w(TRAMP)+time);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start([0x54a930,0x54a9e0,0x54aa80][track],STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert got==bytes(o.mem_read(OUT,4)),(i,got.hex(),bytes(o.mem_read(OUT,4)).hex())
for count,at,rate,track,data,time in [(0,0,15,1,f(1),f(0)),(2,1,15,0,f(1),f(0)),(1,0xffffffff,15,2,f(1),f(0)),(1,0,-1,1,f(1),f(0)),(1,0,15,3,f(1),f(0)),(1,0,15,1,w(0x7fc00000),f(0)),(1,0,15,1,f(1),f(-1))]:
 view=[0]*52;view[30]=rate;view[[31,46,48][min(track,2)]]=count;view[[32,47,49][min(track,2)]]=at;view=w(*view)
 inputs.append(w(len(data),track)+time+view+data);x.mem_write(B,data);x.mem_write(OWNER,view);x.mem_write(OUT,b'\xa5'*4)
 status=call('rf_vfx_material_evaluate',[B,len(data),OWNER,track,struct.unpack('<I',time)[0],OUT]);assert status!=0 and bytes(x.mem_read(OUT,4))==b'\xa5'*4;responses.append(w(status)+b'\xa5'*4)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-material-evaluate'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=7,scope='Borrowed material views, unaligned byte offsets, blend loader clamp, brightness and opacity with original runtime routines. Original loader clamp supplied as normalized input; no native XEMU or embedded frame binding.')
(root/'artifacts/vfx-material-evaluate.json').write_text(json.dumps(report,indent=2));print(report)

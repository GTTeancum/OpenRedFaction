"""Original shadow filter with nonzero, retained subject scratch versus PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_filter_raster\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
assert bytes(o.mem_read(0x409f90,3))==bytes.fromhex('8bc1c3'), 'constructor must leave scratch unchanged'
calls=[]
def observe(u,a,size,ctx):calls.append(a)
o.hook_add(UC_HOOK_CODE,observe,begin=0x4f2100,end=0x4f2100)
rng=random.Random(0x4f53bd);inputs=[];responses=[];accepted_total=changed=0
scratch=f(*[rng.uniform(-20,20) for _ in range(128)])
for i in range(1024):
 n=3+i%6;nr=1+(i//6)%4;width=4+i%20;height=4+(i//20)%20;cx=width*.5;cy=height*.5
 poly=[(cx+(width*.5-1)*math.cos(j*2*math.pi/n),cy+(height*.5-1)*math.sin(j*2*math.pi/n)) for j in range(n)]
 receivers=[];counts=[]
 for k in range(nr):
  nn=3+(i+k)%6;px=rng.uniform(-4,width+4);py=rng.uniform(-4,height+4);r=rng.uniform(.1,10)
  receiver=[(px+r*math.cos(j*2*math.pi/nn),py+r*math.sin(j*2*math.pi/nn)) for j in range(nn)]
  receivers.append(b''.join(f(*v) for v in receiver).ljust(64,b'\0'));counts.append(nn)
 thresholds=[0,.1,2,8,100];threshold=[thresholds[i%5],rng.choice([.5,1,2])];amount=[1,127,255][i%3];capacity=width*(height+1)+1;initial=bytes(rng.randrange(256) for _ in range(1024));rawpoly=b''.join(f(*v) for v in poly).ljust(64,b'\0');counts+= [0]*(4-nr);receivers += [bytes(64)]*(4-nr)
 data=w(n,nr,width,height,capacity,amount,*counts)+f(*threshold)+rawpoly+b''.join(receivers)+initial
 o.mem_write(STACK,bytes(4096));o.mem_write(STACK+0x4f0,scratch);o.mem_write(STACK+0x2f0,rawpoly);o.mem_write(STACK+0x40,w(nr,nr,B+0x1800));o.mem_write(STACK+0x70,w(amount));o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+0x18,w(width,height));o.mem_write(OWNER+0x2c,f(*threshold));o.mem_write(B+0xf00c,w(OWNER));o.mem_write(B+0xf018,w(OUT));o.mem_write(OUT,initial)
 for k in range(nr):o.mem_write(B+0x1000+k*0x200,receivers[k]+bytes(192)+w(counts[k]));o.mem_write(B+0x1800+k*4,w(B+0x1000+k*0x200))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,B+0xf000);o.reg_write(UC_X86_REG_ESI,n);o.reg_write(UC_X86_REG_FPCW,0x27f);calls.clear();o.emu_start(0x4f53bd,0x4f5515,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f5515;expected=bytes(o.mem_read(OUT,1024));accepted=int(bool(calls));assert len(calls)<=1
 x.mem_write(B,data);x.mem_write(B+0xb000,scratch)
 for k in range(4):x.mem_write(B+0x6000+k*8,w(B+112+k*64,counts[k]))
 x.mem_write(B+0x6100,w(B+0x6000,nr)+f(*threshold)+w(B+0x6200,B+0xb000,64));x.mem_write(B+0x6200,w(B+0x8000,B+0x9000,B+0xa000,64));x.mem_write(OUT,w(0xa5a5a5a5))
 status=call(x,entry,[B+48,n,B+0x6100,B+368,capacity,width,height,amount,OUT]);got=bytes(x.mem_read(B+368,1024));got_accepted=struct.unpack('<I',x.mem_read(OUT,4))[0];assert status==0 and got_accepted==accepted and got==expected,(i,status,got_accepted,accepted)
 state=bytes(o.mem_read(STACK+0x4f0,512));assert bytes(x.mem_read(B+0xb000,512))==state,(i,"scratch")
 inputs.append(data+scratch);responses.append(w(0,accepted)+expected+state);scratch=state;accepted_total+=accepted;changed+=sum(a!=b for a,b in zip(initial,expected))
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-filter-warm'],input=b''.join(inputs))==b''.join(responses)
# Reject absent receiver ownership without touching the mask.
bad=bytearray(data);bad[4:8]=w(0);x.mem_write(B,bytes(bad));x.mem_write(B+0x6104,w(0));x.mem_write(OUT,w(0xa5a5a5a5));status=call(x,entry,[B+48,n,B+0x6100,B+368,capacity,width,height,amount,OUT]);assert status!=0 and bytes(x.mem_read(B+368,1024))==initial and bytes(x.mem_read(OUT,4))==w(0xa5a5a5a5)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-filter-warm'],input=bytes(bad)+scratch)==w(status,0xa5a5a5a5)+initial+scratch
report=dict(result='PASS',original_pc_nxdk_passes=len(inputs),accepted=accepted_total,rejected=len(inputs)-accepted_total,changed_mask_bytes=changed,pc_nxdk_empty_receiver_guards=1,original_sha256=sha,x87_control_word='0x027f',scope='Original4f53bd..4f5515 with actual receiver-list lookup,549f10,4f25a0,4f2100; only observation hook at raster entry. One to four receiver polygons, thresholds, full masks and one-fill/early-exit behavior exact. Original aliased subject/output suppresses final clipped-vertex copy; retained subject/tail area behavior reproduced with nonzero scratch retained across1024 passes; full scratch bytes also compared. Original409f90 constructor is MOV EAX,ECX; RET, so does not initialize this array. Full-game stack reuse outside this routine is not modeled. Empty receiver original path references uninitialized raster scratch and is rejected by new API. Occluder selection/ownership and native rendering remain excluded.')
(root/'artifacts/lightmap-shadow-filter-warm.json').write_text(json.dumps(report,indent=2));print(report)

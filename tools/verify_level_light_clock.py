"""VFX scale/quaternion/translation stages against original math helpers."""
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



rng=random.Random(0x45fa30);inputs=[];responses=[];observed=[];draw=0;used=0
P=0xc4e7d8
def hook(cpu,address,size,context):
 global used
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x4d93d0:observed.append(bytes(cpu.mem_read(sp+8,4)));return
 used+=1;cpu.reg_write(UC_X86_REG_EAX,draw);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
o.hook_add(UC_HOOK_CODE,hook,begin=0x57312d,end=0x57312d);o.hook_add(UC_HOOK_CODE,hook,begin=0x4d93d0,end=0x4d93d0)
for i in range(4096):
 flags=((1+i%4)<<8)|(2 if i%3 else 0);enabled=i%5!=0;draw=rng.randrange(32768);cycle=[rng.random()*3,rng.random(),rng.random(),rng.random(),rng.random(),rng.random()];phase=i%2;elapsed=rng.random();delay=rng.choice([0,.1,1,2]);seconds=rng.choice([0,.01,.1,1,10]);state=w(phase)+f(elapsed,delay,.33)+w(99,99);data=w(flags,enabled,draw)+f(*cycle,seconds)+state;inputs.append(data);x.mem_write(B,data);assert call('rf_level_light_clock_step',[flags,enabled,B+12,read(x,B+36),draw,B+40,OUT])==0;got=bytes(x.mem_read(OUT,24));responses.append(w(0)+got)
 o.mem_write(OWNER,bytes(160));o.mem_write(P,bytes(268));o.mem_write(P+8,w(2));o.mem_write(OWNER+0x3c,w((flags>>8)&15));o.mem_write(OWNER+0x44,f(.2,.3,.4));o.mem_write(OWNER+0x6c,f(cycle[0],cycle[3],cycle[1],cycle[2],cycle[4],cycle[5]));o.mem_write(OWNER+0x87,bytes([bool(flags&2)]));o.mem_write(OWNER+0x88,state[8:12]+state[4:8]);o.mem_write(OWNER+0x90,bytes([phase,enabled]));o.mem_write(0x879af8,b'\0');observed.clear();used=0
 o.mem_write(STACK,w(STOP)+f(seconds));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x45fa30,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 expected=w(o.mem_read(OWNER+0x90,1)[0])+bytes(o.mem_read(OWNER+0x8c,4))+bytes(o.mem_read(OWNER+0x88,4))+(observed[-1] if observed else state[12:16])+w(bool(observed),used)
 assert got==expected,(i,got.hex(),expected.hex())
 assert call('rf_level_light_clock_step',[flags,enabled,B+12,read(x,B+36),draw,B+40,B+40])==0 and bytes(x.mem_read(B+40,24))==got
for guard in range(2):
 data=bytearray(inputs[-1]);data[:12]=w(0x300,1,32768 if guard else 0);data[36:40]=f(10);data[40:44]=w(2 if not guard else 0);data[48:52]=f(0);inputs.append(bytes(data));x.mem_write(B,bytes(data));x.mem_write(OUT,b'\xa5'*24)
 status=call('rf_level_light_clock_step',[read(x,B),1,B+12,read(x,B+36),read(x,B+8),B+40,OUT]);assert status!=0 and bytes(x.mem_read(OUT,24))==b'\xa5'*24;responses.append(w(status)+b'\xa5'*24)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-light-clock'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=4096,nxdk_in_place_cases=4096,guards=2,scope='Complete45fa30 plus actual4d93d0 color update. Only RNG integer draw supplied; intensity arguments observed without replacing calls. Exact phase/elapsed/delay/intensity/update flags, static/disabled cases, interpolation and single-transition overshoot. Live timers/RNG sequencing remain to bind.')
(root/'artifacts/level-light-clock.json').write_text(json.dumps(report,indent=2));print(report)

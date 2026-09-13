"""Original ambient selection and accumulation seeding vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VIEW=B+0x6000;OWNER=B+0x7000;IMAGE=B+0x7100;DIRTY=B+0x7200;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_seed_ambient\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

ROOM_OWNER=B+0x8000;TABLE=B+0x8100;ROOM=B+0x9000;rng=random.Random(0x4f2841);inputs=[];responses=[]
for i in range(512):
 width=1+i%16;height=1+(i//16)%16;global_rgb=[rng.uniform(-2,2) for j in range(3)];room=bytes([i%4]+[rng.randrange(256) for j in range(3)]);data=w(width,height,256)+f(*global_rgb)+room;inputs.append(data)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+24,w(width,height));o.mem_write(OWNER+104,w(0 if i%5 else 0xffffffff));o.mem_write(ROOM_OWNER+0x90,w(1,1,TABLE));o.mem_write(TABLE,w(ROOM));o.mem_write(ROOM+0x45,room);o.mem_write(0x5a38d4,f(*global_rgb))
 # Exercise room absence with an inactive override in the matching API input.
 if i%5==0:data=data[:24]+bytes([0])+data[25:];inputs[-1]=data
 for a in [0x1431de0,0x14b23e0,0x13f1de0]:o.mem_write(a,bytes([165])*1024)
 o.mem_write(STACK,bytes(128));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EBX,ROOM_OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f2841,0x4f2972,count=100000)
 expected=b''.join(bytes(o.mem_read(a,1024)) for a in [0x1431de0,0x14b23e0,0x13f1de0])
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*3072);x.mem_write(VIEW,bytes(56)+w(width,height,0,0,0,0)+f(0)+w(OUT,OUT+1024,OUT+2048,256))
 assert call([VIEW,B+12,B+24])==0 and bytes(x.mem_read(OUT,3072))==expected,i;responses.append(w(0)+expected)
probe=[str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-seed-ambient']
assert subprocess.check_output(probe,input=b''.join(inputs))==b''.join(responses)
for field,value in [(56,0),(60,0),(96,0),(84,0)]:
 saved=bytes(x.mem_read(VIEW+field,4));x.mem_write(VIEW+field,w(value));x.mem_write(OUT,bytes([165])*3072)
 assert call([VIEW,B+12,B+24])!=0 and bytes(x.mem_read(OUT,3072))==bytes([165])*3072;x.mem_write(VIEW+field,saved)
report=dict(result='PASS',original_pc_nxdk_grids=len(inputs),nxdk_guards=4,original_sha256=sha,x87_control_word='0x027f',scope='Original4f2841..4f2972 ambient room/global selection and plane fill, actual room-array lookup and4d8d10; no hooks. Every1..16 dimension pair twice, room absence/flags0..3, finite signed global RGB and unchanged tails. Zero-light direct RGB fill and ambient ownership are excluded.')
(root/'artifacts/lightmap-seed-ambient.json').write_text(json.dumps(report,indent=2));print(report)

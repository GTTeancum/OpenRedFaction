"""Original4091d0/409210 with real409190 versus shared PC/NXDK reset."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
B=0x30000000;INV=B+0x4000;BE=INV+0x100;STACK=B+0xe000;STOP=B+0xf000;STUB=STOP+0x100;WEIGHT=STOP+0x200
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');u.mem_write(STUB,b'\xd9\x05'+w(WEIGHT)+b'\xc3')
entry=int(re.search(r'\s_rf_entity_ai_reset_motion\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
traces=[[],[]];cfg=[];failure=-1;calls=0
callbacks=(0x428d10,0x5033d0,0x503400);compiled=(STOP+0x300,STOP+0x400,STOP+0x500)
def hook(cpu,address,size,context):
 global calls
 original=cpu is u;ids=callbacks if original else compiled
 if address not in ids:return
 calls+=1;op=ids.index(address);sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(cpu,sp+4+i*4)
 a=arg(0 if original else 1);motion=arg(1 if original else 2) if op<2 else 0
 traces[0 if original else 1].extend((op,(a-B)//0x2000 if op==0 else a,motion))
 if not original and calls==failure:
  cpu.reg_write(UC_X86_REG_EAX,0xffffffff);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
 if cfg[3 if op<2 else 4]:cpu.mem_write(INV,w(B+0x2000))
 if original and op==1:cpu.reg_write(UC_X86_REG_EIP,STUB);return
 value=cfg[1] if op==0 else 0
 if not original:
  if op==0:cpu.mem_write(arg(3),w(value))
  elif op==1:cpu.mem_write(arg(3),struct.pack('<d',struct.unpack('<f',w(cfg[2]))[0]))
  value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
def call(cpu,address,args):
 cpu.mem_write(STACK,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,STACK);cpu.emu_start(address,STOP,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==STOP and cpu.reg_read(UC_X86_REG_ESP)==STACK+4;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x409210);commands=[];expected=[];offsets=(0x80,0x810,0x834,0x1364,0x1368)
weights=(0,0x80000000,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc00000,1)
for case in range(2048):
 seeds=[bytearray(rng.randbytes(0x1500)) for _ in range(2)];wire=[]
 for i,seed in enumerate(seeds):
  seed[0x80:0x84]=w(700+i);seed[0x1364:0x136c]=w(rng.choice((-1,0,9)),rng.choice((-1,0,10)))
  values=[struct.unpack_from('<I',seed,o)[0] for o in offsets];wire.extend(values)
  u.mem_write(B+i*0x2000,bytes(seed));x.mem_write(B+i*0x2000,w(*values))
 cfg=[(case//8)%2,(0,1,2,256,257)[case%5],weights[case%8],(case//16)%2,(case//32)%2];wire+=cfg
 for cpu in (u,x):cpu.mem_write(INV,w(B))
 u.mem_write(WEIGHT,w(cfg[2]));x.mem_write(BE,w(*compiled,0));traces=[[],[]];calls=0
 call(u,0x409210 if cfg[0] else 0x4091d0,[INV]);out=[]
 for i,seed in enumerate(seeds):
  after=bytearray(u.mem_read(B+i*0x2000,len(seed)));out.extend(read(u,B+i*0x2000+o) for o in offsets)
  for o in offsets:after[o:o+4]=seed[o:o+4]
  assert after==seed,('original unexpected write',case)
 status=call(x,entry,[INV,cfg[0],BE]);actual=[read(x,B+i*0x2000+o*4) for i in range(2) for o in range(5)]
 assert status==0 and actual==out and read(x,INV)==read(u,INV) and traces[0]==traces[1],case
 result=w(0,(read(u,INV)-B)//0x2000,*out,len(traces[0])//3,*traces[0],*([0]*(24-len(traces[0]))))
 commands.append(w(*wire));expected.append(result)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-reset'],input=b''.join(commands))
assert actual==b''.join(expected),'PC mismatch'
cfg=[1,1,0x3f800000,0,0]
for failure in range(1,5):
 x.mem_write(B,w(700,0xffffffff,123,9,10));x.mem_write(INV,w(B));x.mem_write(BE,w(*compiled,0));calls=0;traces=[[],[]]
 assert call(x,entry,[INV,1,BE])==0xffffffff and calls==failure
 assert read(x,B+16)==(0xffffffff if failure>=3 else 10)
 assert read(x,B+12)==9
failure=-1
print(dict(result='PASS',original_pc_nxdk_cases=2048,callback_failure_cases=4,scope='Full4091d0/409210 plus409190, only playback active/remaining-time/stop supplied. Exact complete original state footprints and shared compact fields, callback ordering, owner rebinding, NaN/zero/infinite query results and low-byte active results. No native scene integration.'))
(root/'artifacts/ai-motion-reset.json').write_text(json.dumps(dict(result='PASS',cases=2048,callback_failures=4),indent=2))

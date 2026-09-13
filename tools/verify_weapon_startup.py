"""Original422cf5 startup weapon sequence against PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_EBP
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
word=lambda c,a:struct.unpack('<I',bytes(c.mem_read(a,4)))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);c.mem_write(p.OPTIONAL_HEADER.ImageBase,im);c.mem_map(B,0x10000);return c
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_weapon_startup_grant_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
calls=0;after=fail=0
def hook(c,at,size,original):
 global calls
 sp=c.reg_read(UC_X86_REG_ESP)
 if original:assert word(c,sp+4)==B+0x4000;weapon=word(c,sp+8);assert weapon==word(c,B+0x42a4)==word(c,B+0x7088);c.mem_write(B+0x7088,w(after))
 else:weapon=word(c,sp+8);assert weapon==word(c,B+448)==word(c,B+456);c.mem_write(B+456,w(after))
 calls+=1;c.reg_write(UC_X86_REG_EAX,0xffffffff if not original and fail else 0);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook,True,begin=0x42ab20,end=0x42ab20);x.hook_add(UC_HOOK_CODE,hook,False,begin=B+0xa000,end=B+0xa000)
def original(blob):
 global calls
 u.mem_write(B,bytes(0x9000));u.mem_write(B+0x42a0,w(B+0x4000));u.mem_write(B+0x442c,blob[:64]);u.mem_write(B+0x42ac,blob[64:192]);u.mem_write(B+0x432c,blob[192:448]);u.mem_write(B+0x42a4,blob[448:456]);u.mem_write(B+0x7088,blob[456:468]);u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,b'\0')
 for i in range(64):
  ammo,cap,clip=struct.unpack_from('<iii',blob,468+12*i);u.mem_write(0x85cd2c+i*0x550,w(ammo));u.mem_write(0x85cf68+i*0x550,w(cap));u.mem_write(0x85cd90+i*0x550,w(clip))
 u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ESI,B+0x4000);u.reg_write(UC_X86_REG_EBX,B+0x42a0);u.reg_write(UC_X86_REG_EDI,B+0x7000);u.reg_write(UC_X86_REG_EBP,0xffffffff);calls=0;u.emu_start(0x422cf5,0x422dbd,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x422dbd
 return w(0)+bytes(u.mem_read(B+0x442c,64))+bytes(u.mem_read(B+0x42ac,128))+bytes(u.mem_read(B+0x432c,256))+bytes(u.mem_read(B+0x42a4,8))+bytes(u.mem_read(B+0x7088,12))+w(calls)
def compiled(blob):
 global calls
 x.mem_write(B,blob);x.mem_write(STACK,w(STOP,B,B+448,B+456,B+468,B+0xa000,0));x.reg_write(UC_X86_REG_ESP,STACK);calls=0;x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,468))+w(calls)
rng=random.Random(0x422cf5);cases=[];expected=[];equips=0
for n in range(1024):
 defaults=[rng.choice([-1,0,1,2,63]) for _ in range(3)];after=rng.choice([0,1,2,63]);fail=0
 blob=bytes(rng.choice([0,0,0,1,255]) for _ in range(64))+w(*[rng.randint(-10,100) for _ in range(96)])+w(rng.choice([-1,3]),rng.choice([-1,4]),*defaults)+b''.join(w(rng.choice([-1,0,1,31]),rng.choice([0,10,100]),rng.choice([0,1,16,30])) for _ in range(64))+w(after,fail)
 result=original(blob);equips+=struct.unpack('<I',result[-4:])[0];actual=compiled(blob);assert actual==result,(n,defaults,after,result.hex(),actual.hex());cases.append(blob);expected.append(result)
after=0;fail=1;blob=bytes(448)+w(-1,-1,0,1,2)+w(0,10,3)*64+w(after,fail);result=compiled(blob);out=bytearray(blob[:468]);out[0]=1;out[192:196]=w(3);out[448:452]=w(0);assert result==w(-1)+out+w(1);cases.append(blob);expected.append(result)
for defaults in ((64,-1,-1),(-1,64,-1),(-1,-1,64)):
 after=0;fail=0;blob=bytes(448)+w(-1,-1,*defaults)+w(0,10,3)*64+w(after,fail);result=compiled(blob);assert result==w(-4)+blob[:468]+w(0);cases.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--startup'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=1024,equips=equips,callback_failures=1,bounds_guards=3,scope='Original422cf5..422dbd including real4030d0/401470/42cce0/4895d0. Only primary42ab20 equip/reset boundary supplied. Complete inventory and primary/secondary/defaults compared, repeated weapon IDs, no-ammo types, preowned state, post-equip default reread, refill and extra grant ordering. Prior inventory/AI construction and actual equip resources excluded.')
(root/'artifacts/weapon-startup.json').write_text(json.dumps(report,indent=2));print(report)

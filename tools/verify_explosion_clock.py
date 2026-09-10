"""Original central explosion timing and release ordering vs PC/NXDK."""
import hashlib,json,re,struct,random,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
base=0x30000000;stack=base+0xe000;stop=base+0xf000;obj=base+0x1000;recipe_at=base+0x2000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,65536);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'_rf_explosion_clock_tick\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def w(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def f(a,v):u.mem_write(a,struct.pack('<f',v))
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
calls=[]
def hook(m,a,s,d):
 if a not in (0x4972f0,0x497d80):return
 sp=m.reg_read(UC_X86_REG_ESP);pointer=m.reg_read(UC_X86_REG_ECX) if a==0x4972f0 else read(sp+4)
 slot=(pointer-base-0x3000)//64;assert 0<=slot<6;calls.append((int(a==0x497d80),slot))
 m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(48);commands=bytearray();expected=bytearray();expired=0
for case in range(700):
 count=case%7;live=rng.randrange(1<<count);active=int(case%17!=0)
 elapsed=(case%7)/4;dt=[0,0.25,-0.25,0.0625][case%4];size=[0,0.5,1,2][case%4];duration=[0.5,1,1.5,2][case%4]
 factors=[rng.randrange(0,33)/16 for _ in range(6)];modes=[rng.choice([0,1,2,257]) for _ in range(6)]
 recipe=bytearray(712);struct.pack_into('<I',recipe,36,count);struct.pack_into('<f',recipe,40,duration)
 u.mem_write(obj,bytes(0x134));u.mem_write(recipe_at,bytes(152));w(0x75e518,obj if active else 0);w(0x75ec40,0);w(obj+0x12c,obj);w(obj+0x130,obj);w(obj+0x128,recipe_at);f(obj+0x20,elapsed);f(obj+0x1c,size);f(0x5a4014,dt);w(recipe_at+0x7c,count);f(recipe_at+0x88,duration);f(recipe_at+0x8c,-3e38);w(recipe_at+0x28,-1)
 for slot in range(count):
  struct.pack_into('<Iff',recipe,108+slot*76,modes[slot],0,factors[slot]);u.mem_write(recipe_at+0x44+slot,bytes([modes[slot]&255]));f(recipe_at+0x64+slot*4,factors[slot]);w(obj+0x110+slot*4,base+0x3000+slot*64 if live&(1<<slot) else 0)
 calls.clear();w(stack,stop);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x48e290,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 assert calls==sorted(calls),calls
 process=sum(1<<slot for kind,slot in calls if kind==0);release=sum(1<<slot for kind,slot in calls if kind==1)
 remaining=sum(1<<slot for slot in range(count) if read(obj+0x110+slot*4));still_active=int(read(0x75e518)!=0);expired+=active and not still_active
 result=bytes(4)+bytes(u.mem_read(obj+0x20,4))+struct.pack('<f4I',size,remaining,still_active,process,release);expected.extend(result)
 state=struct.pack('<ffII',elapsed,size,live,active);commands.extend(recipe+struct.pack('<f',dt)+state)
 x.mem_write(base,bytes(recipe));x.mem_write(base+0x4000,state+bytes([0xa5])*8);x.mem_write(stack,struct.pack('<IIfII',stop,base,dt,base+0x4000,base+0x4010));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(4)+bytes(x.mem_read(base+0x4000,24))==result,case
probe=root/'build/pc/Release/rf_effect_probe.exe';assert subprocess.check_output([str(probe),'--explosion-clock'],input=commands)==expected
report=dict(result='PASS',cases=700,expired=expired,original_sha256=sha,scope='Full 48e290 with one circular active record or empty list, trail path disabled, central process/release calls intercepted without mutation. PC/NXDK elapsed, action masks, release state and process-before-release slot order match. No particles, trail processing, callback mutation or frame-phase integration.')
(root/'artifacts/explosion-clock-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

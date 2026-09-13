"""Original41b4c0 aim arithmetic with resolved predicates and registry services."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
f=lambda v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;STACK=B+0xff000;STOP=B+0xfff00;ENTITY=B+0x10000;TARGET=B+0x20000;ACTOR=B+0x30000;OUT=B+0x4000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;c.mem_map(a,(len(im)+4095)//4096*4096);c.mem_write(a,im);c.mem_map(B,0x100000);return c
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_target_aim\s+([0-9a-fA-F]+)',mapping)[1],16)
def word(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
facts=[];trace=[]
def service(c,a,size,data):
 sp=c.reg_read(UC_X86_REG_ESP);arg=word(c,sp+4);trace.append(a)
 if a==0x48aaf0:assert arg==ENTITY;value=facts[0]
 elif a==0x428700:assert arg==ENTITY;value=facts[1]
 elif a==0x40a0e0:assert arg==123;value=TARGET if facts[2] else 0
 else:assert arg==456;value=ACTOR if facts[3] else 0
 c.reg_write(UC_X86_REG_EAX,value);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x48aaf0,0x428700,0x40a0e0,0x426fc0):u.hook_add(UC_HOOK_CODE,service,begin=a,end=a)
def run(c,a,args):
 c.mem_write(STACK,w(STOP,*args));c.reg_write(UC_X86_REG_ESP,STACK);c.reg_write(UC_X86_REG_FPCW,0x27f);c.emu_start(a,STOP,count=50000);assert c.reg_read(UC_X86_REG_EIP)==STOP;return c.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x41b4c0);inputs=[];outputs=[];changed=0
for k in range(2048):
 facts[:]=[rng.choice([0,1,2,256,257]),rng.choice([0,1,2,256]),rng.randrange(2),rng.randrange(2)]
 pos=[rng.uniform(-10,10) for _ in range(3)];eye=[rng.uniform(-10,10) for _ in range(3)];muzzle=[0,0,0];eb=[1,0,0,0,1,0,0,0,1];ib=[rng.uniform(-2,2) for _ in range(9)]
 if k%2:
  facts[:]=[0,0,1,k%3!=0];pos=[rng.uniform(-.2,.2),rng.uniform(-.2,.2),rng.uniform(1,10)];eye=[v*2 for v in pos]
  eb[3:6]=[rng.uniform(-1,1),rng.uniform(.1,1),rng.uniform(-1,1)]
 if k%19==0:eb[3:6]=[0,0,0]
 if k%23==0:pos=eye=[0,0,1];eb[3:6]=[0,0,1]
 if k%29==0:pos=eye=[0,0,0]
 if k%31==0:
  facts[:]=[0,0,1,0];pos=eye=[.6,0,.8+((k//31)%3-1)*2**-24];eb=[1,0,0,0,1,0,0,0,1]
 source=w(*facts)+f(pos+eye+eb);u.mem_write(ENTITY+0x560,w(123));u.mem_write(TARGET+0x2c,w(456));u.mem_write(TARGET+0x3c,f(pos));u.mem_write(ACTOR+0x7d4,f(eye));u.mem_write(ENTITY+0x7e0,f(eb));u.mem_write(B+0x1000,f(muzzle));u.mem_write(OUT,f(ib));trace.clear()
 run(u,0x41b4c0,[ENTITY,B+0x1000,OUT]);expected=bytes(u.mem_read(OUT,36));changed+=expected!=f(ib)
 wanted=[0x48aaf0]
 if facts[0]&255!=1:
  wanted+=[0x428700]
  if not facts[1]&255:
   wanted+=[0x40a0e0]
   if facts[2]:wanted+=[0x426fc0]
 assert trace==wanted
 x.mem_write(B,source);x.mem_write(B+0x1000,f(muzzle));x.mem_write(OUT,f(ib));status=run(x,entry,[B,B+0x1000,OUT]);got=bytes(x.mem_read(OUT,36))
 assert status==0 and got==expected,(k,facts,struct.unpack('<9f',got),struct.unpack('<9f',expected))
 inputs.append(source+f(muzzle+ib));outputs.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--target-aim'],input=b''.join(inputs))==b''.join(outputs)
# Defined port guards: null pointers and nonfinite reached target geometry.
valid=w(0,0,1,0)+f([0,0,1,0,0,1,1,0,0,0,1,0,0,0,1]);x.mem_write(B,valid);x.mem_write(B+0x1000,f([0,0,0]))
for args in ([0,B+0x1000,OUT],[B,0,OUT],[B,B+0x1000,0]):
 x.mem_write(OUT,f(ib));assert run(x,entry,args)==0xfffffffc and bytes(x.mem_read(OUT,36))==f(ib)
for offset in (16,16+8,16+24+24):
 x.mem_write(B,valid);x.mem_write(B+offset,w(0x7fc00000));x.mem_write(OUT,f(ib));assert run(x,entry,[B,B+0x1000,OUT])==0xfffffffc and bytes(x.mem_read(OUT,36))==f(ib)
report=dict(result='PASS',cases=2048,nxdk_guards=6,changed=changed,original_sha256=digest,scope='Original41b4c0 and normalization/dot/basis construction unhooked; only48aaf0/428700 predicates and40a0e0/426fc0 lookups supplied and checked. Includes target actor eye override, low-byte predicates, absent targets, zero direction, zero/parallel up. Exact PC/NXDK at x87 027f. Predicate side effects and live firing excluded.')
(root/'artifacts/weapon-target-aim.json').write_text(json.dumps(report,indent=2));print(report)

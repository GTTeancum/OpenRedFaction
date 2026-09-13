"""Original428d90 reserve ammunition addition against PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
word=lambda c,a:struct.unpack('<I',bytes(c.mem_read(a,4)))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);c.mem_write(p.OPTIONAL_HEADER.ImageBase,im);c.mem_map(B,0x10000);return c
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_weapon_add_ammo\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=0;values=[]
def hook(c,at,size,original):
 global trace
 stage=1 if at in (0x425250,B+0xa000) else 2;trace=trace*10+stage;sp=c.reg_read(UC_X86_REG_ESP)
 if original:
  assert word(c,sp+4)==B
  if stage==2:assert word(c,sp+8)==0 and word(c,sp+12)==0
 elif stage==1:c.mem_write(word(c,sp+8),w(values[8]))
 else:assert word(c,sp+8)==0 and word(c,sp+12)==0
 result=(values[8] if stage==1 else 0) if original else (-1 if values[9]==stage else 0)
 c.reg_write(UC_X86_REG_EAX,result&0xffffffff);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x425250,0x425280):u.hook_add(UC_HOOK_CODE,hook,True,begin=at,end=at)
for at in (B+0xa000,B+0xa010):x.hook_add(UC_HOOK_CODE,hook,False,begin=at,end=at)
def original(blob):
 global trace
 current,pending,count,ammo,capacity,magazine,weapon,quantity,reloading,fail=values
 u.mem_write(B,bytes(0x2000));u.mem_write(B+0x42c,blob[:64]);u.mem_write(B+0x2ac,blob[64:192]);u.mem_write(B+0x32c,blob[192:448]);u.mem_write(B+0x2a4,w(current));u.mem_write(B+0x13ac,w(pending));u.mem_write(0x872448,w(count));u.mem_write(0x85cd2c+weapon*0x550,w(ammo));u.mem_write(0x85cf68+weapon*0x550,w(capacity));u.mem_write(0x85cd90+weapon*0x550,w(magazine));u.mem_write(STACK,w(STOP,B,weapon,quantity));u.reg_write(UC_X86_REG_ESP,STACK);trace=0;u.emu_start(0x428d90,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return w(0)+bytes(u.mem_read(B+0x42c,64))+bytes(u.mem_read(B+0x2ac,128))+bytes(u.mem_read(B+0x32c,256))+w(word(u,B+0x2a4),word(u,B+0x13ac),count,trace)
def compiled(blob):
 global trace
 x.mem_write(B,blob[:472]);x.mem_write(B+0x1000,w(0,B+0xa000,B+0xa010));x.mem_write(STACK,w(STOP,B,B+448,B+460,values[6],values[7],B+0x1000));x.reg_write(UC_X86_REG_ESP,STACK);trace=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,460))+w(trace)
rng=random.Random(0x428d90);cases=[];expected=[];routes={}
for n in range(2048):
 weapon=rng.randrange(64);blob=bytes(rng.randrange(256) for _ in range(64))+w(*[rng.choice([-2147483648,-1,0,1,20,2147483647]) for _ in range(96)])+w(rng.choice([weapon,weapon,-1]),rng.choice([-2147483648,-1,0,20,2147483647]),rng.choice([0,weapon,weapon+1,64]),rng.choice([-1,0,31]),rng.choice([-1,0,1,100,2147483647]),rng.choice([-1,0,1,30]),weapon,rng.choice([-2147483648,-1,0,1,100,2147483647]),rng.choice([0,1,2,255,256,257]),0);values=list(struct.unpack('<10i',blob[448:]));result=original(blob);actual=compiled(blob);assert actual==result,(n,values,result.hex(),actual.hex());cases.append(blob);expected.append(result);t=struct.unpack('<I',result[-4:])[0];routes[t]=routes.get(t,0)+1
for stage in (1,2):
 blob=bytes(448)+w(0,5,64,0,10,30,0,100,1,stage);values=list(struct.unpack('<10i',blob[448:]));result=compiled(blob);assert result==w(-1)+bytes(64)+w(100 if stage==1 else 10)+bytes(380)+w(0,5 if stage==1 else 105,64,1 if stage==1 else 12);cases.append(blob);expected.append(result)
for weapon,ammo in ((-1,0),(64,0),(0,32)):
 blob=bytes(448)+w(0,5,64,ammo,10,30,weapon,100,1,0);values=list(struct.unpack('<10i',blob[448:]));result=compiled(blob);assert result==w(-4)+blob[:460]+w(0);cases.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--add-ammo'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=2048,routes=routes,callback_failures=2,bounds_guards=3,scope='Complete428d90 and actual4c86e0 magazine predicate.425250 reload-state query and425280 reload service are boundaries. Exact full inventory, current/pending, signed wrapping, negative reserve normalization, cap ordering, active weapon count and low-byte query results. Query/reload internals and scene binding excluded.')
(root/'artifacts/weapon-ammo.json').write_text(json.dumps(report,indent=2));print(report)

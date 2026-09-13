"""Original world-model/grip/muzzle accessors versus PC/NXDK ownership cache."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
B=0x30000000;STACK=B+0xff000;STOP=B+0xfff00;CALL=B+0xf0000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;u.mem_map(a,(len(im)+4095)//4096*4096);u.mem_write(a,im);u.mem_map(B,0x100000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_world_tag\s+([0-9a-fA-F]+)',mapping)[1],16)
state={};calls=[0,0]
def readword(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
def lookup(c,a,size,mode):
 sp=c.reg_read(UC_X86_REG_ESP);model=readword(c,sp+(4 if mode==0 else 8));name=readword(c,sp+(8 if mode==0 else 12))
 expected=b'muzzle_1' if state['kind'] else b'grip_1';assert bytes(c.mem_read(name,len(expected)+1))==expected+b'\0';assert model==state['model'];calls[mode]+=1
 if mode: c.mem_write(readword(c,sp+16),w(state['value']))
 c.reg_write(UC_X86_REG_EAX,(state['value'] if mode==0 else state['status'])&0xffffffff);c.reg_write(UC_X86_REG_EIP,readword(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,lookup,user_data=0,begin=0x503220,end=0x503220);x.hook_add(UC_HOOK_CODE,lookup,user_data=1,begin=CALL,end=CALL)
rng=random.Random(0x4c8510);inputs=[];outputs=[];lookups=0
for k in range(1024):
 weapon=rng.choice([-1,64,rng.randrange(64)]);kind=k&1;value=rng.choice([-2,-1,0,7,123]);models=[[rng.choice([0,1,2]),rng.choice([0,0x1234,0x5678]),rng.choice([-2,-1,-1,0,19]),rng.choice([-2,-1,-1,0,27])] for _ in range(64)]
 for repeat in range(2):
  raw=b''.join(w(*row) for row in models);state.update(kind=kind,value=value,status=0,model=models[weapon][1] if 0<=weapon<64 and models[weapon][0] else 0);calls[:]=[0,0]
  for i,row in enumerate(models):u.mem_write(0x85cd08+i*0x550+0x28,w(0,B+0x8000 if row[0] else 0,row[1],row[2],row[3]))
  u.mem_write(B+0x8000,b'weapon.v3d\0');u.mem_write(STACK,w(STOP,weapon));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x4c8560 if kind else 0x4c8510,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP
  tag=u.reg_read(UC_X86_REG_EAX)
  expected_models=[row[:] for row in models]
  if 0<=weapon<64:expected_models[weapon][2 if kind else 3]=struct.unpack('<i',u.mem_read(0x85cd08+weapon*0x550+(0x34 if kind else 0x38),4))[0]
  expected=w(0,tag,calls[0])+b''.join(w(*row) for row in expected_models)
  x.mem_write(B,raw);x.mem_write(B+0x9000,w(-99));x.mem_write(STACK,w(STOP,B,weapon,kind,CALL,0,B+0x9000));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
  got=w(x.reg_read(UC_X86_REG_EAX),readword(x,B+0x9000),calls[1])+bytes(x.mem_read(B,1024));assert got==expected,(k,repeat)
  inputs.append(raw+w(weapon,kind,value,0));outputs.append(expected);lookups+=calls[0];models=expected_models
# Missing service and callback failure preserve output/cache. Invalid kind also preserves output.
for kind,callback,error in ((0,0,-3),(0,CALL,-1),(2,CALL,-4)):
 models=[[0,0,-1,-1] for _ in range(64)];models[2]=[1,0x1234,-1,-1];raw=b''.join(w(*r) for r in models);state.update(kind=kind,value=17,status=error,model=0x1234);calls[:]=[0,0]
 x.mem_write(B,raw);x.mem_write(B+0x9000,w(-99));x.mem_write(STACK,w(STOP,B,2,kind,callback,0,B+0x9000));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=10000)
 assert x.reg_read(UC_X86_REG_EAX)==error&0xffffffff and bytes(x.mem_read(B,1024))==raw and readword(x,B+0x9000)==0xffffff9d
 if callback:inputs.append(raw+w(2,kind,17,error));outputs.append(w(error,-99,calls[1])+raw)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--world-tags'],input=b''.join(inputs))==b''.join(outputs)
report=dict(result='PASS',original_cases=2048,lookup_calls=lookups,port_guards=3,original_sha256=digest,scope='Original4c8510/4c8560 and actual4c84d0/4ff490; only503220 model-tag lookup supplied and arguments checked. PC/NXDK exact cached state and return, repeated missing tags, empty names, absent models, invalid IDs and retained non-minus-one caches. Model loading, transforms and drawing excluded.')
(root/'artifacts/weapon-world-tags.json').write_text(json.dumps(report,indent=2));print(report)

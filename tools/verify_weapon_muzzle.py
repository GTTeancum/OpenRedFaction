"""Compare complete original41b5a0 muzzle orchestration with C PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
f=lambda v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;STACK=B+0xff000;STOP=B+0xfff00;TAG=B+0xf0000;TRANS=TAG+16;AIM=TAG+32
SRC=B;MODELS=B+0x1000;OUT=B+0x3000;OPS=B+0x4000;ENTITY=B+0x10000;CLASS=B+0x20000;SECOND=B+0x30000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;u.mem_map(a,(len(im)+4095)//4096*4096);u.mem_write(a,im);u.mem_map(B,0x100000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_muzzle_pose\s+([0-9a-fA-F]+)',mapping)[1],16)
def word(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={};calls=[0,0,0]
def service(c,a,size,mode):
 sp=c.reg_read(UC_X86_REG_ESP);args=[word(c,sp+4+4*i) for i in range(7)]
 if mode:args=args[1:]
 if a in (TAG,0x503220):
  calls[0]+=1;assert args[0]==state['model'] and bytes(c.mem_read(args[1],9))==b'muzzle_1\0'
  if mode:c.mem_write(args[2],w(state['tag']))
  value=0 if mode else state['tag']
 elif a in (TRANS,0x5034f0):
  n=calls[1];calls[1]+=1
  if n==0:
   assert args[:2]==[state['actor'],state['hand']]
   assert bytes(c.mem_read(args[2],36))==state['basis'] and bytes(c.mem_read(args[3],12))==state['pos']
   c.mem_write(args[4],state['hand_basis']);c.mem_write(args[5],state['hand_point'])
  else:
   assert n==1 and args[:2]==[state['model'],state['resolved']&0xffffffff]
   assert bytes(c.mem_read(args[2],36))==state['hand_basis'] and bytes(c.mem_read(args[3],12))==state['hand_point']
   c.mem_write(args[4],b'\x7e'*36);c.mem_write(args[5],state['muzzle_point'])
  value=0
 else:
  calls[2]+=1
  if not mode:assert args[0]==ENTITY;args=args[1:]
  assert bytes(c.mem_read(args[0],12))==state['muzzle_point'] and bytes(c.mem_read(args[1],36))==state['hand_basis']
  c.mem_write(args[1],state['aim_basis']);value=0
 if mode and state.get("fail")==sum(calls):value=-1
 c.reg_write(UC_X86_REG_EAX,value&0xffffffff);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for addr in (0x503220,0x5034f0,0x41b4c0):u.hook_add(UC_HOOK_CODE,service,user_data=0,begin=addr,end=addr)
for addr in (TAG,TRANS,AIM):x.hook_add(UC_HOOK_CODE,service,user_data=1,begin=addr,end=addr)
def run(c,addr,args):
 c.mem_write(STACK,w(STOP,*args));c.reg_write(UC_X86_REG_ESP,STACK);c.reg_write(UC_X86_REG_FPCW,0x27f);c.emu_start(addr,STOP,count=30000);assert c.reg_read(UC_X86_REG_EIP)==STOP;return c.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x41b5a0);inputs=[];outputs=[];totals=[0,0,0];fallback=0
for k in range(2048):
 weapon=rng.choice([-1,64,rng.randrange(64)]);limit=32;count=rng.randrange(3);pi=rng.randrange(2);si=rng.randrange(2);hands=[11,12,21,22];actor=0x123456
 rows=[[0,0,-1,-1] for _ in range(64)];model=0x1234;cached=rng.choice([-2,-1,0,9]);tag=rng.choice([-1,0,13])
 if 0<=weapon<64:rows[weapon]=[rng.choice([0,1,1,1]),rng.choice([0,model,model,model]),cached,-1]
 model=rows[weapon][1] if 0<=weapon<64 and rows[weapon][0] else 0
 floats=lambda n:f([rng.uniform(-100,100) for _ in range(n)])
 pos=floats(3);basis=floats(9);eye=floats(3);eye_basis=floats(9);hb=floats(9);hp=floats(3);mp=floats(3);ab=floats(9)
 source=w(actor,weapon,limit,pi,si,count,*hands)+pos+basis+eye+eye_basis;raw=b''.join(w(*r) for r in rows)
 state.update(actor=actor,hand=hands[pi if weapon<limit else 2+si],model=model,tag=tag,resolved=cached if cached!=-1 else tag,pos=pos,basis=basis,hand_basis=hb,hand_point=hp,muzzle_point=mp,aim_basis=ab)
 u.mem_write(0x87211c,w(limit));u.mem_write(ENTITY+0x29c,w(CLASS));u.mem_write(ENTITY+0x294,w(SECOND));u.mem_write(CLASS+0x1d4,w(count,*hands[:2]));u.mem_write(SECOND+0x1e0,w(2,*hands[2:]));u.mem_write(ENTITY+0x50c,w(si,pi));u.mem_write(ENTITY+0x80,w(actor));u.mem_write(ENTITY+0x3c,pos+basis);u.mem_write(ENTITY+0x7d4,eye+eye_basis)
 for i,row in enumerate(rows):u.mem_write(0x85cd08+i*0x550+0x28,w(0,B+0x8000 if row[0] else 0,row[1],row[2],row[3]))
 u.mem_write(B+0x8000,b'weapon.v3d\0');u.mem_write(OUT,b'\xa5'*48);calls[:]=[0,0,0];run(u,0x41b5a0,[ENTITY,weapon,OUT,OUT+12])
 updated=[r[:] for r in rows]
 for i,row in enumerate(updated):row[2]=word(u,0x85cd08+i*0x550+0x34)
 expected=w(0)+bytes(u.mem_read(OUT,48))+b''.join(w(*r) for r in updated)+w(*calls)
 totals=[a+b for a,b in zip(totals,calls)];fallback+=calls[1]==0
 x.mem_write(SRC,source);x.mem_write(MODELS,raw);x.mem_write(OUT,b'\xa5'*48);x.mem_write(OPS,w(TAG,TRANS));calls[:]=[0,0,0]
 status=run(x,entry,[SRC,MODELS,OPS,AIM,0,OUT,OUT+12]);got=w(status)+bytes(x.mem_read(OUT,48))+bytes(x.mem_read(MODELS,1024))+w(*calls)
 assert got==expected,(k,weapon,next((i for i,(a,b) in enumerate(zip(got,expected)) if a!=b),None),calls)
 inputs.append(source+raw+hb+hp+mp+ab+w(tag));outputs.append(expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--muzzle'],input=b''.join(inputs))==b''.join(outputs)
# Reached callback errors preserve the completed output prefix and cache rules.
rows=[[0,0,-1,-1] for _ in range(64)];rows[2]=[1,0x1234,-1,-1];raw=b''.join(w(*r) for r in rows)
source=w(actor,2,32,0,0,1,11,12,21,22)+pos+basis+eye+eye_basis
state.update(actor=actor,hand=11,model=0x1234,tag=13,resolved=13,pos=pos,basis=basis,hand_basis=hb,hand_point=hp,muzzle_point=mp,aim_basis=ab)
for failed in range(1,5):
 state['fail']=failed;calls[:]=[0,0,0];x.mem_write(SRC,source);x.mem_write(MODELS,raw);x.mem_write(OUT,b'\xa5'*48);x.mem_write(OPS,w(TAG,TRANS))
 status=run(x,entry,[SRC,MODELS,OPS,AIM,0,OUT,OUT+12]);assert status==0xffffffff and sum(calls)==failed
 assert bytes(x.mem_read(OUT,48))==(mp+ab if failed==4 else b'\xa5'*48)
 assert word(x,MODELS+2*16+8)==(0xffffffff if failed==1 else 13)
state['fail']=0
for args in ([0,MODELS,OPS,AIM,0,OUT,OUT+12],[SRC,MODELS,OPS,AIM,0,0,OUT+12],[SRC,MODELS,OPS,AIM,0,OUT,0]):
 x.mem_write(OUT,b'\xa5'*48);assert run(x,entry,args)==0xfffffffc and bytes(x.mem_read(OUT,48))==b'\xa5'*48
report=dict(result='PASS',cases=2048,port_failure_guards=7,fallback=fallback,calls=totals,original_sha256=digest,scope='Complete original41b5a0, actual model/tag getters and hand-list accessors; only model tag lookup/transforms and41b4c0 aim supplied with checked arguments. Stable source inputs, both weapon families, separate secondary owner, missing models/tags and primary-count fallback. PC/NXDK bytes exact at x87 0x27f. Live firing/aim implementation excluded.')
(root/'artifacts/weapon-muzzle.json').write_text(json.dumps(report,indent=2));print(report)

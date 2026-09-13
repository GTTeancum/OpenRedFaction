"""Execute original418e60 and compiled ports with checked model services."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
f=lambda v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;STACK=B+0xff000;STOP=B+0xfff00;TAG=B+0xf0000;TRANS=TAG+16
SRC=B;MODELS=B+0x1000;OUT=B+0x3000;OPS=B+0x4000;ENTITY=B+0x10000;CLASS=B+0x20000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;u.mem_map(a,(len(im)+4095)//4096*4096);u.mem_write(a,im);u.mem_map(B,0x100000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_place_in_hand\s+([0-9a-fA-F]+)',mapping)[1],16)
def word(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
def signed(v):return struct.unpack('<i',w(v))[0]
def token(rows,weapon):return rows[weapon][1] if 0<=weapon<64 and rows[weapon][0] else 0
state={};calls=[0,0,0]
def service(c,a,size,mode):
 sp=c.reg_read(UC_X86_REG_ESP);offset=8 if mode else 4
 args=[word(c,sp+offset+4*i) for i in range(6)]
 model,tag=args[:2];calls[2]+=1
 if a in (TAG,0x503220):
  calls[1]+=1
  assert model==token(state['rows'],state['next'])
  assert bytes(c.mem_read(tag,7))==b'grip_1\0'
  if mode:c.mem_write(args[2],w(state['grip']))
  value=state['grip'] if not mode else (-1 if state['fail']==calls[2] else 0)
 else:
  n=calls[0];calls[0]+=1
  if n==0:
   assert model==state['actor'] and signed(tag)==state['hands'][state['hand']]
   assert bytes(c.mem_read(args[2],36))==state['src_basis'] and bytes(c.mem_read(args[3],12))==state['pos']
   c.mem_write(args[4],state['basis']);c.mem_write(args[5],state['point'])
   c.mem_write(SRC+4 if mode else ENTITY+0x2a4,w(state['next']))
  else:
   assert n==1 and model==state['model'] and signed(tag)==state['resolved_grip']
   assert bytes(c.mem_read(args[2],36))==state['basis'] and bytes(c.mem_read(args[3],12))==state['point']
   c.mem_write(args[5],state['grip_point'])
  value=-1 if mode and state['fail']==calls[2] else 0
 c.reg_write(UC_X86_REG_EAX,value&0xffffffff);c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for addr in (0x503220,0x5034f0):u.hook_add(UC_HOOK_CODE,service,user_data=0,begin=addr,end=addr)
for addr in (TAG,TRANS):x.hook_add(UC_HOOK_CODE,service,user_data=1,begin=addr,end=addr)
def run(c,addr,args):
 c.mem_write(STACK,w(STOP,*args));c.reg_write(UC_X86_REG_ESP,STACK);c.emu_start(addr,STOP,count=30000);assert c.reg_read(UC_X86_REG_EIP)==STOP;return signed(c.reg_read(UC_X86_REG_EAX))
def port(source,raw,hand,ops=True):
 calls[:]=[0,0,0];x.mem_write(SRC,source);x.mem_write(MODELS,raw);x.mem_write(OUT,b'\xa5'*60);x.mem_write(OPS,w(TAG,TRANS))
 status=run(x,entry,[SRC,hand,MODELS,OPS if ops else 0,0,OUT])
 return w(status)+bytes(x.mem_read(OUT,60))+bytes(x.mem_read(MODELS,1024))+bytes(x.mem_read(SRC,68))+w(*calls)
# Class4246e0 appends primary_weapon_N through42d930, whose actual capacity is2.
for count in range(3):
 for value in range(32):
  initial=w(count,71,83,0xdeadbeef);u.mem_write(CLASS,initial);u.reg_write(UC_X86_REG_ECX,CLASS)
  code=run(u,0x42d930,[value]);expected=bytearray(initial)
  if count<2:expected[:4]=w(count+1);expected[4+count*4:8+count*4]=w(value)
  assert code==(count if count<2 else -1) and bytes(u.mem_read(CLASS,16))==bytes(expected)
rng=random.Random(0x418e60);inputs=[];outputs=[];total=[0,0,0]
for k in range(2048):
 rows=[[rng.choice([0,1,1]),rng.choice([0,0x1234,0x5678]),-1,rng.choice([-2,-1,-1,0,19])] for _ in range(64)]
 weapon=rng.choice([-1,64,rng.randrange(64)]);nxt=rng.choice([-1,64,rng.randrange(64),weapon]);count=rng.randrange(3);hand=rng.randrange(3);hands=[rng.randrange(-2,90) for _ in range(2)]
 if k%4:
  weapon=k%64;nxt=(weapon+1)%64;count=2;hand=k%2
  rows[weapon][0:2]=[1,0x1234];rows[nxt][0:2]=[1,0x5678]
 floats=lambda n:f([rng.uniform(-100,100)*rng.choice([1,1,1,1e12,1e-12]) for _ in range(n)])
 pos=floats(3);src_basis=floats(9);basis=floats(9);point=floats(3);grip_point=floats(3);grip=rng.choice([-2,-1,0,12]);actor=0x123456
 source=w(actor,weapon,count,*hands)+pos+src_basis;raw=b''.join(w(*r) for r in rows)
 resolved=rows[nxt][3] if token(rows,nxt) else -1
 if resolved==-1 and token(rows,nxt):resolved=grip
 state.update(rows=rows,actor=actor,hands=hands,hand=hand,next=nxt,model=token(rows,weapon),src_basis=src_basis,pos=pos,basis=basis,point=point,grip_point=grip_point,grip=grip,resolved_grip=resolved,fail=0)
 u.mem_write(ENTITY+0x29c,w(CLASS));u.mem_write(CLASS+0x1d4,w(count,*hands));u.mem_write(ENTITY+0x80,w(actor));u.mem_write(ENTITY+0x2a4,w(weapon));u.mem_write(ENTITY+0x3c,pos+src_basis)
 for i,row in enumerate(rows):u.mem_write(0x85cd08+i*0x550+0x28,w(0,B+0x8000 if row[0] else 0,row[1],row[2],row[3]))
 u.mem_write(B+0x8000,b'weapon.v3d\0');u.mem_write(OUT,b'\xa5'*60);calls[:]=[0,0,0]
 status=run(u,0x418e60,[ENTITY,hand,OUT,OUT+12,OUT+24]);assert status in (0,-1)
 updated=[row[:] for row in rows]
 for i,row in enumerate(updated):row[3]=signed(word(u,0x85cd08+i*0x550+0x38))
 expected=w(0 if status==0 else -3)+bytes(u.mem_read(OUT,60))+b''.join(w(*r) for r in updated)+source[:4]+w(word(u,ENTITY+0x2a4))+source[8:]+w(*calls)
 total=[a+b for a,b in zip(total,calls)]
 got=port(source,raw,hand);assert got==expected,(k,next((i for i,(a,b) in enumerate(zip(got,expected)) if a!=b),None))
 inputs.append(source+raw+w(hand)+basis+point+grip_point+w(nxt,grip,0));outputs.append(expected)
# Port safety and service error prefixes; original transform service has no status return.
rows=[[0,0,-1,-1] for _ in range(64)];rows[3]=[1,0x1234,-1,-1];rows[4]=[1,0x5678,-1,-1];raw=b''.join(w(*r) for r in rows)
state.update(rows=rows,next=4,model=0x1234,grip=12,resolved_grip=12,hand=0)
for fail,hand,count in [(1,0,1),(2,0,1),(3,0,1),(0,-1,1),(0,0,3)]:
 state['fail']=fail;source=w(actor,3,count,*hands)+pos+src_basis
 got=port(source,raw,hand);status=signed(struct.unpack_from('<I',got)[0]);assert status==(-1 if fail else -4)
 expected_placement=point+b'\xa5'*48 if fail==1 else point+point+basis if fail else b'\xa5'*60
 expected_rows=[r[:] for r in rows]
 if fail==3:expected_rows[4][3]=12
 expected_source=source[:4]+w(4)+source[8:] if fail else source
 expected_calls={1:[1,0,1],2:[1,1,2],3:[2,1,3],0:[0,0,0]}[fail]
 expected=w(status)+expected_placement+b''.join(w(*r) for r in expected_rows)+expected_source+w(*expected_calls)
 assert got==expected
 inputs.append(source+raw+w(hand)+basis+point+grip_point+w(4,12,fail));outputs.append(expected)
state['fail']=0;source=w(actor,3,1,*hands)+pos+src_basis
assert port(source,raw,0,False)==w(-3)+b'\xa5'*60+raw+source+w(0,0,0)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--hand-placement'],input=b''.join(inputs))
expected=b''.join(outputs);assert actual==expected,next((i//1168 for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),('length',len(actual),len(expected)))
report=dict(result='PASS',original_cases=2048,original_hand_capacity_cases=96,service_calls=total,port_guards=6,original_sha256=digest,scope='Full original418e60 with actual list, vector, model and grip-cache helpers; only5034f0 transform and503220 tag services supplied with arguments checked. Exact PC/NXDK outputs, caches, source mutation, captured model versus reread weapon, two float stores, and port error prefixes. Model transforms/loading and scene rendering excluded.')
(root/'artifacts/weapon-hand.json').write_text(json.dumps(report,indent=2));print(report)

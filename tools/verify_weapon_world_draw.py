"""Original421c40 visibility/model prefix against PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[v&0xffffffff for v in v])
B=0x30000000;S=B+0xff000;STOP=B+0xfff00;E=B+0x10000;C=B+0x20000;P=B+0x22000;L=B+0x25000;LC=B+0x26000;M=B+0x30000;O=B+0x40000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;u.mem_map(a,(len(im)+4095)//4096*4096);u.mem_write(a,im);u.mem_map(B,0x100000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_world_draw_run\s+([0-9a-fA-F]+)',mp)[1],16)
def get(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={};calls=0
assert get(u,0x486cc8)==0x486c9f
u.mem_write(L+0x24,w(0));u.mem_write(L+0x294,w(LC))
def lookup(c,a,size,data):
 global calls
 calls+=1;sp=c.reg_read(UC_X86_REG_ESP);assert get(c,sp+4)==55
 c.reg_write(UC_X86_REG_EAX,L if state['kind']>=0 else 0);c.reg_write(UC_X86_REG_EIP,get(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
def finish(c,a,size,data):c.emu_stop()
u.hook_add(UC_HOOK_CODE,lookup,begin=0x426fc0,end=0x426fc0)

u.reg_write(UC_X86_REG_FPCW,0x27f);x.reg_write(UC_X86_REG_FPCW,0x27f)
OPS=B+0x6000;PLACE=B+0x6100;DRAW=B+0x6200
counts=[0,0];records=[]
def callback(c,at,size,port):
 sp=c.reg_read(UC_X86_REG_ESP)
 if at in (0x418e60,PLACE):
  hand=get(c,sp+8);assert hand<2;state['hand']=hand;counts[0]+=1
  missing=state['missing']&(1<<hand);pose=state['poses'][hand]
  if counts[0]==1 and state['missing']&256:c.mem_write(B+44 if port else C+0x1d4,w(0))
  if not missing:
   if port:c.mem_write(get(c,sp+12),pose)
   else:
    c.mem_write(get(c,sp+12),pose[:12]);c.mem_write(get(c,sp+16),pose[12:24]);c.mem_write(get(c,sp+20),pose[24:])
  code=(-3 if port else -1) if missing else 0
 else:
  counts[1]+=1
  if port:
   model=get(c,sp+8);pose=bytes(c.mem_read(get(c,sp+12),60));scratch=bytes(c.mem_read(get(c,sp+16),80))
  else:
   model=get(c,sp+4);pose=state['poses'][state['hand']][:12]+bytes(c.mem_read(get(c,sp+8),12))+bytes(c.mem_read(get(c,sp+12),36));scratch=bytes(c.mem_read(get(c,sp+16),80))
  records.append(w(model)+pose+scratch);code=0
  if counts[1]==1 and state['missing']&512:
   c.mem_write(B+44 if port else C+0x1d4,w(2));addr=B if port else E+0x810;c.mem_write(addr,w(get(c,addr)^256))
 c.reg_write(UC_X86_REG_EAX,code&0xffffffff);c.reg_write(UC_X86_REG_EIP,get(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for addr in (0x418e60,0x503100):u.hook_add(UC_HOOK_CODE,callback,False,begin=addr,end=addr)
for addr in (PLACE,DRAW):x.hook_add(UC_HOOK_CODE,callback,True,begin=addr,end=addr)
x.mem_write(OPS,w(PLACE,DRAW));rng=random.Random(0x421c41);inputs=[];outputs=[];totals=[0,0]
for k in range(1024):
 flags=rng.getrandbits(32)&~0x801;cl=8;inv=0;weapon=rng.choice([-1,3,3,3]);attachment=-1;kind=rng.choice([-1,0,1,4]);present=rng.randrange(2);p44=rng.randrange(3);p3c=rng.randrange(3);special=3;override=rng.choice([0,0x123456])
 if k%7==0:flags|=1
 count=rng.randrange(3);recoil=struct.pack('<f',rng.uniform(-1,1) if k%3 else 0);mode=rng.choice([0,1,2,257]);tint=rng.getrandbits(32);missing=rng.randrange(4)|(rng.randrange(3)<<8)
 view=w(flags,cl,inv,weapon,attachment,kind,present,p44,p3c,special,override);draw=view+w(count)+recoil+w(mode,tint)
 poses=[struct.pack('<15f',*[rng.uniform(-2,2) for _ in range(15)]) for _ in range(2)];scratch=w(*[rng.getrandbits(32) for _ in range(20)]);rows=[[1,0x9876,-1,-1] for _ in range(64)];models=b''.join(w(*r) for r in rows)
 state.update(kind=kind,poses=poses,missing=missing)
 u.mem_write(E+0x810,w(flags));u.mem_write(E+0x294,w(C));u.mem_write(C+0x724,w(cl));u.mem_write(E+0x7d0,w(inv));u.mem_write(E+0x2a4,w(weapon));u.mem_write(E+0x75c,w(attachment));u.mem_write(E+0x200,w(55));u.mem_write(LC+0x1b4,w(kind))
 u.mem_write(E+0x1430,w(P if present else 0));u.mem_write(P+0x1044,bytes([p44]));u.mem_write(P+0x103c,bytes([p3c]));u.mem_write(0x87210c,w(special));u.mem_write(0x872478,w(override))
 u.mem_write(E+0x29c,w(C));u.mem_write(C+0x1d4,w(count));u.mem_write(E+0x13c4,recoil);u.mem_write(E+0x1474,w(tint));u.mem_write(0x7c763c,w(B+0x5000));u.mem_write(B+0x5fb0,bytes([mode&255]))
 for i,r in enumerate(rows):u.mem_write(0x85cd08+i*0x550+0x28,w(0,B+0x8000,r[1]))
 u.mem_write(B+0x8000,b'model.v3d\0');u.mem_write(S-0x98,scratch);u.mem_write(S,w(STOP,E));u.reg_write(UC_X86_REG_ESP,S);counts[:]=[0,0];records.clear();u.emu_start(0x421c40,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 expected=w(0,get(u,E+0x810))+draw[4:44]+w(get(u,C+0x1d4))+draw[48:]+bytes(u.mem_read(S-0x98,80))+w(*counts)+b''.join(records).ljust(288,b'\0');totals=[a+b for a,b in zip(totals,counts)]
 x.mem_write(B,draw);x.mem_write(M,models);x.mem_write(O,scratch);x.mem_write(S,w(STOP,B,M,O,OPS,0));x.reg_write(UC_X86_REG_ESP,S);counts[:]=[0,0];records.clear();x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,60))+bytes(x.mem_read(O,80))+w(*counts)+b''.join(records).ljust(288,b'\0');assert got==expected,(k,next((i for i,(a,b) in enumerate(zip(got,expected)) if a!=b),None))
 inputs.append(draw+models+b''.join(poses)+w(missing)+scratch);outputs.append(expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--world-draw'],input=b''.join(inputs))==b''.join(outputs)
report=dict(result='PASS',cases=1024,placements=totals[0],submissions=totals[1],scope='Full original421c40 with original predicates, recoil, state initialization and final flags; only linked handle lookup, placement and submission supplied. Exact PC/NXDK state, callback records, missing hands, empty lists, player overrides and preserved scratch. Callback count shrink/growth and flag mutation are exercised; renderer execution remains separate.')
(root/'artifacts/weapon-world-draw.json').write_text(json.dumps(report,indent=2));print(report)

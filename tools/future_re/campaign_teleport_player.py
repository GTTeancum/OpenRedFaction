"""Execute complete original Teleport_Player63 with real pose/matrix helpers."""
import sys,struct,json,hashlib,itertools
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'local/python'))
(ROOT/'artifacts/future-campaign-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
EXE=ROOT/'Installed_Game/RF.exe';SHA=hashlib.sha256(EXE.read_bytes()).hexdigest();assert SHA=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
IMAGE=pefile.PE(str(EXE)).get_memory_mapped_image();BASE=0x30000000;STACK=BASE+0x7e000;STOP=BASE+0x7f000
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine():
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(IMAGE)+4095)//4096*4096);u.mem_write(0x400000,IMAGE);u.mem_map(BASE,0x80000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
def word(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def ret(u,value=0):
 sp=u.reg_read(UC_X86_REG_ESP);r=word(u,sp);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,r)
levels=json.loads((ROOT/'artifacts/events.json').read_text())['results'];authored=[]
for level in levels:
 for r in level['records']:
  if r['type_index']==63:authored.append(dict(level=level['file'],**r))
(ROOT/'artifacts/future-campaign-re/teleport-player-authored.json').write_text(json.dumps(authored,indent=2)+'\n')
rotations=[('identity',[1,0,0,0,1,0,0,0,1]),('yaw90',[0,0,-1,0,1,0,1,0,0])]
for r in authored:
 if r['level'] in ('L6S3.rfl','L11S3.rfl'):rotations.append((r['level']+':'+str(r['uid']),r['orientation_disk'][3:]+r['orientation_disk'][:3]))
results=[]
for name,basis in rotations:
 for present,host,special,detach,mp,server,local in [(0,0,0,0,0,0,0),(1,0,0,0,0,0,1),(1,1,0,0,0,0,1),(1,1,0,1,0,0,1),(1,1,1,0,0,0,1),(1,0,0,0,1,0,1),(1,0,0,0,1,1,1),(1,0,0,0,0,0,0)]:
  u=machine();entity=BASE+0x10000;vehicle=BASE+0x20000;event=BASE+0x30000;trace=[];target=vehicle if host and special else entity
  for obj in (entity,vehicle):
   u.mem_write(obj,b'\0'*0x2000);u.mem_write(obj+0x144,f(1,2,3,4,5,6));u.mem_write(obj+0x180,f(2));u.mem_write(obj+0x7c,w(0x100));u.mem_write(obj+0x200,w(0x1234))
  u.mem_write(0x5cb054,w(entity if present else 0));u.mem_write(0x64ecb9,bytes([mp,server]));u.mem_write(event+0x40,f(12.5,-7,3.25));u.mem_write(event+0x4c,f(*basis))
  def hook(cpu,a,n,data):
   if a in (0x426fc0,0x5001d0,0x4279d0,0x42a0d0,0x4a0770,0x4a3740):
    sp=cpu.reg_read(UC_X86_REG_ESP);argc=2 if a==0x5001d0 else 1;args=list(struct.unpack('<'+'I'*argc,cpu.mem_read(sp+4,argc*4)));trace.append(dict(address=hex(a),args=args))
    value={0x426fc0:vehicle if host else 0,0x5001d0:special,0x4279d0:detach,0x42a0d0:local,0x4a0770:0,0x4a3740:0}[a];ret(cpu,value)
  u.hook_add(UC_HOOK_CODE,hook);u.mem_write(STACK,w(STOP,event));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x4b9820,STOP,count=1000000)
  assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==STACK+4
  if present:
   for off in (0x3c,0xe4,0xf0):assert bytes(u.mem_read(target+off,12))==f(12.5,-7,3.25)
   for off in (0x48,0xfc,0x120,0x7e0):assert bytes(u.mem_read(target+off,36))==f(*basis)
   assert bytes(u.mem_read(target+0x144,24))==f(1,2,3,4,5,6)
   assert word(u,target+0x7c)==0x4000100
   assert bytes(u.mem_read(target+0x190,12))==f(10.5,-9,1.25) and bytes(u.mem_read(target+0x19c,12))==f(14.5,-5,5.25)
  detached=[x for x in trace if x['address']=='0x4279d0'];assert len(detached)==int(present and host and not special)
  notified=[x for x in trace if x['address']=='0x4a0770'];assert len(notified)==int(present and not(mp and not server) and local)
  results.append(dict(rotation=name,present=present,host=host,special_L20S2=special,detach_return=detach,multiplayer=mp,server=server,local_predicate=local,target='host' if host and special else 'player',angles=list(struct.unpack('<3f',u.mem_read(target+0x864,12))),trace=trace))
report=dict(result='PASS',cases=len(results),original_sha256=SHA,authored_records=len(authored),scope='Complete4b9820,48a230, vector/matrix copies, bounds update, real4fc060/504ea0 angle extraction and42d840 store. Host lookup, level-string predicate, detach and network/player notification boundaries intercepted. Detach AL0/AL1 both tested; no room/query/renderer or shared implementation executed.',results=results)
(ROOT/'artifacts/future-campaign-re/teleport-player.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'Teleport_Player cases;',len(authored),'authored uses')

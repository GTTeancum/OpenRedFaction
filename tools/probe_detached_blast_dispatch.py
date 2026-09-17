"""Execute original blast list dispatch and kind3 direct damage/lifecycle gates."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe'
sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe=pefile.PE(str(exe));image=pe.get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
B=0x30000000;STACK=B+0xe0000;STOP=B+0xf0000;ORIGIN=B+0x8000
u.mem_map(B,0x100000)
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
word=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
mode='dispatch';calls=[]
def ret(value=0):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
def hook(cpu,address,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if mode=='dispatch':
  if address in (0x4290d0,0x40a110):ret(0)
  elif address==0x489010:calls.append(word(sp+4));ret()
  elif address==0x491f50:calls.append('separate_effect_mesh_service');ret()
 elif mode=='damage':
  if address==0x40a0e0:ret(B)
  elif address in (0x426fc0,0x48aaf0):ret(0)
 elif mode=='lifecycle' and address==0x4fa3f0:ret(0) # non-expired lifetime
u.hook_add(UC_HOOK_CODE,hook)
def run(entry,args):
 u.mem_write(STACK,w(STOP)+args);u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
rows=[]
# Actual kind3 birth413288 inserts into5c98e8, which is NOT one of488dc0's lists.
lists=[(0x5cb060,0),(0x5c9360,4),(0x5cabb8,2),(0x872128,7),(0x5c98e8,3)]
for populated in (False,True):
 calls.clear();u.mem_write(B,bytes(0x9000));u.mem_write(0x64ecb9,b'\0')
 for index,(sentinel,kind) in enumerate(lists):
  obj=B+index*0x1000
  u.mem_write(sentinel+0x28c,w(obj if populated or kind==3 else sentinel))
  u.mem_write(obj+0x24,w(kind));u.mem_write(obj+0x28c,w(sentinel))
  u.mem_write(obj+0x190,f(-1,-1,-1,1,1,1))
  if kind==7:u.mem_write(obj+0x294,w(B+0x6000));u.mem_write(B+0x6264,w(0x40))
 run(0x488dc0,w(ORIGIN)+f(400,5)+w(0x123,3))
 expected=[B+i*0x1000 for i in range(4)] if populated else []
 assert calls==expected+['separate_effect_mesh_service'],calls
 rows.append(dict(mode='dispatch',other_lists_populated=populated,kind3_dispatched=False,other_victims=len(expected)))
# Full generic direct damage, ordinary single-player, no player association.
mode='damage'
for amount in (0,1,99.999,100,100.00001,200,400):
 u.mem_write(B,bytes(0x3000));u.mem_write(B+0x24,w(3));u.mem_write(B+0x34,f(250))
 u.mem_write(B+0x1a8,w(0x1800003f));u.mem_write(B+0x144,f(0,0,0))
 u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,b'\0')
 amount=struct.unpack('<f',f(amount))[0]
 run(0x4892c0,w(0x123)+f(amount)+w(0xffffffff,0xffffffff,3,0,0xffffffff,0))
 health=struct.unpack('<f',u.mem_read(B+0x34,4))[0]
 expected=struct.unpack('<f',f(250-amount))[0] if amount>100 else 250
 assert health==expected,(amount,health,expected)
 assert word(B+0x1a8)==0x1800003f and bytes(u.mem_read(B+0x144,12))==f(0,0,0)
 mode='lifecycle';run(0x412ad0,w(B));retired=bool(word(B+0x7c)&2)
 assert retired==(health<=0)
 rows.append(dict(mode='direct_damage',amount=amount,health=health,retired=retired,body_woken=False));mode='damage'
folder=ROOT/'artifacts/geomod-postedit-re';folder.mkdir(parents=True,exist_ok=True)
report=dict(result='PASS',original_sha256=sha,cases=rows,scope='Real488dc0 loops/AABB,4892c0 kind3 arithmetic,412ad0 life retirement. Registry/actor predicates and effect-mesh service supplied; no original game launch, no blast impulse or chunk subdivision proved.')
(folder/'detached-blast-dispatch.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

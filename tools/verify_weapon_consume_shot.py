"""Original4257c0 and its unhooked magazine predicate versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
B=0x30000000;STACK=B+0xff000;STOP=B+0xfff00;INV=B+0x1000;DEFS=B+0x2000;ENTITY=B+0x10000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;c.mem_map(a,(len(im)+4095)//4096*4096);c.mem_write(a,im);c.mem_map(B,0x100000);return c
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_weapon_consume_shot\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def run(c,a,args):
 c.mem_write(STACK,w(STOP,*args));c.reg_write(UC_X86_REG_ESP,STACK);c.emu_start(a,STOP,count=10000);assert c.reg_read(UC_X86_REG_EIP)==STOP;return c.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4257c0);inputs=[];outputs=[];loaded=reserve=noop=0
for k in range(2048):
 weapon=rng.choice([-1,64,65,rng.randrange(64),rng.randrange(64)]);count=rng.randrange(65)
 rows=[[rng.randrange(32),rng.randrange(1000),rng.choice([-1,0,1,10,30])] for _ in range(64)]
 inventory=bytes(rng.randrange(256) for _ in range(64))+w(*[rng.choice([-2147483648,-9,-1,0,1,2,30,2147483647]) for _ in range(96)])
 raw=b''.join(w(*r) for r in rows);u.mem_write(ENTITY+0x42c,inventory[:64]);u.mem_write(ENTITY+0x2ac,inventory[64:]);u.mem_write(0x872448,w(count))
 for i,row in enumerate(rows):u.mem_write(0x85cd08+i*0x550+0x24,w(row[0]));u.mem_write(0x85cd08+i*0x550+0x88,w(row[2]))
 run(u,0x4257c0,[ENTITY,weapon]);expected=w(0)+bytes(u.mem_read(ENTITY+0x42c,64))+bytes(u.mem_read(ENTITY+0x2ac,384))
 x.mem_write(INV,inventory);x.mem_write(DEFS,raw);status=run(x,entry,[INV,DEFS,count,weapon]);got=w(status)+bytes(x.mem_read(INV,448));assert got==expected,k
 inputs.append(inventory+raw+w(count,weapon));outputs.append(expected)
 if not 0<=weapon<64:noop+=1
 elif weapon<count and rows[weapon][2]>0:loaded+=1
 else:reserve+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--consume-shot'],input=b''.join(inputs))==b''.join(outputs)
# Invalid reserve map must not write outside inventory. Magazine branch ignores map.
guards=[]
for ammo,magazine,count,expected in [(-1,0,64,-4),(32,0,64,-4),(-1,1,64,0),(32,1,64,0),(0,0,65,-4)]:
 rows=[[ammo,100,magazine] for _ in range(64)];raw=b''.join(w(*r) for r in rows);x.mem_write(INV,inventory);x.mem_write(DEFS,raw);status=run(x,entry,[INV,DEFS,count,2]);assert status==expected&0xffffffff
 if expected:assert bytes(x.mem_read(INV,448))==inventory
 guards.append(inventory+raw+w(count,2));outputs_guard=w(expected)+bytes(x.mem_read(INV,448))
 assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--consume-shot'],input=guards[-1])==outputs_guard
for args in ([0,DEFS,64,2],[INV,0,64,2]):assert run(x,entry,args)==0xfffffffc
assert run(x,entry,[0,0,65,64])==0
report=dict(result='PASS',cases=2048,loaded=loaded,reserve=reserve,no_op=noop,pc_nxdk_guards=5,nxdk_null_guards=3,original_sha256=digest,scope='Complete4257c0 plus actual4c86e0, no hooks. Full448-byte inventory exact across PC/NXDK; invalid IDs, magazine/reserve selection, count boundary and signed wrapping before zero clamp. Port bounds guards exclude undefined original reserve indices. Does not implement firing eligibility, timing or projectile creation.')
(root/'artifacts/weapon-consume-shot.json').write_text(json.dumps(report,indent=2));print(report)

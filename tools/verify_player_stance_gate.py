"""Original 430c70 stance gates versus shared PC and compiled NXDK C.
World environment lookup/transition are supplied; object/mode/global predicates run.
"""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000
player,entity,cls,mode,command,stack,stop=[base+n for n in (0,0x2000,0x4000,0x6000,0x8000,0xe000,0xf000)]
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(im)+4095)//4096*4096);u.mem_write(origin,im);u.mem_map(base,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);environment=0;reached=False
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def hook(cpu,address,size,data):
 global reached
 if address in (0x430daa,0x430e19):
  reached=address==0x430daa;cpu.emu_stop();return
 if address not in (0x45cca0,0x4281e0,0x4280b0):return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x45cca0:cpu.reg_write(UC_X86_REG_EAX,0x1234 if environment else 0)
 cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)
u.mem_write(0x7394cc,w(entity));u.mem_write(entity+0x24,w(0));u.mem_write(entity+0x2c,w(0x10000))
u.mem_write(entity+0x294,w(cls));u.mem_write(entity+0x858,w(mode))
u.mem_write(0x64ecb9,b'\x01')
commands=[];expected=[]
for ownership,environment,movement,speed,kind,attached,locked,global_lock in itertools.product(range(3),(0,1),range(16),(0,1,2),(0,1),(-1,0),(0,255),(0,1)):
 u.mem_write(player+0x14,w(-1 if ownership==0 else 0x10000))
 u.mem_write(entity+0x1430,w(player if ownership==2 else 0))
 u.mem_write(mode+4,w(movement));u.mem_write(entity+0x8c4,w(speed))
 u.mem_write(cls+0x1b4,w(kind));u.mem_write(entity+0x1380,w(attached))
 u.mem_write(player+0xf38,bytes([locked]));u.mem_write(0x63c3b8,w(global_lock))
 u.mem_write(stack,w(stop,player,command));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 reached=False;u.emu_start(0x430c70,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP) in (0x430daa,0x430e19)
 commands.append(w(int(ownership==2),environment,movement,speed,kind,attached,locked,global_lock));expected.append(int(reached))
probe=root/'build/pc/Release/rf_entity_probe.exe'
assert subprocess.check_output([str(probe),'--player-stance-gate'],input=b''.join(commands))==w(*expected)
binary=root/'build/xbox/main.exe';x=load(binary)
entry=int(re.search(r'_rf_player_stance_enabled\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for data,want in zip(commands,expected):
 x.mem_write(base,data);x.mem_write(stack,w(stop,base));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=1000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==want and bytes(x.mem_read(base,32))==data
x.mem_write(stack,w(stop,0));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=1000)
assert x.reg_read(UC_X86_REG_EAX)==0
report=dict(result='PASS',cases=len(commands),enabled=sum(expected),original_sha256=sha,
 pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='Original 430c70 prefix to action-4 query with unchanged ownership lookup, mode/kind and global predicates. Environment lookup result and environment transitions supplied. Resolved C gate matches PC/NXDK; excludes action processing, environment mutation and live owner registration.')
(root/'artifacts/player-stance-gate-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

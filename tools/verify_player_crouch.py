"""Original player crouch eligibility with real lookup/predicate callees versus C."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536)
player,entity,control,parent,info,mode,stack,stop=[base+n for n in (0,0x2000,0x4000,0x6000,0x8000,0xa000,0xe000,0xf000)]
pack=lambda values:struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values])
def put(a,*v):u.mem_write(a,pack(v))
u.mem_write(0x7394cc,bytes(4096))
for slot,obj in enumerate((entity,control,parent)):
    put(0x7394cc+slot*4,obj);put(obj+0x24,0);put(obj+0x2c,0x10000+slot);put(obj+0x294,info+slot*1024)
put(entity+0x858,mode)
commands=[];expected=[]
for present,control_kind,parent_kind,attached,movement in itertools.product((0,1),(-1,0,1,4,5,6),(-1,0,1,4,5,6),(-1,0,0x10003),range(16)):
    put(player+0x14,0x10000 if present else -1)
    put(player+0xb4,0x10001 if control_kind!=-1 else -1)
    put(entity+0x200,0x10002 if parent_kind!=-1 else -1)
    put(entity+0x75c,attached);put(mode+4,movement)
    put(info+1024+0x1b4,control_kind);put(info+2048+0x1b4,parent_kind)
    before=bytes(u.mem_read(base,0xc000))
    put(stack,stop,player);u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x4a5c50,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and bytes(u.mem_read(base,0xc000))==before
    expected.append(u.reg_read(UC_X86_REG_EAX)&255)
    commands.append(pack([present,control_kind,parent_kind,attached,movement]))
probe=root/'build/pc/Release/rf_entity_probe.exe'
assert subprocess.check_output([str(probe),'--player-crouch'],input=b''.join(commands))==pack(expected)
binary=root/'build/xbox/main.exe';pe=pefile.PE(str(binary));image=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(image)+4095)//4096*4096);x.mem_write(origin,image);x.mem_map(base,65536)
entry=int(re.search(r'_rf_player_can_crouch\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for command,want in zip(commands,expected):
    x.mem_write(base,command);x.mem_write(stack,pack([stop,base]));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=1000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==want and bytes(x.mem_read(base,20))==command
report=dict(result='PASS',cases=len(commands),allowed=sum(expected),original_sha256=sha,
 pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='Complete original 4a5c50 with unchanged object lookups and kind predicates; explicit registered object/class fixtures, no hooks. PC/NXDK resolved eligibility matches, including missing objects, mode 0..15, control/parent kinds and attachment handles. Does not verify outer input/ownership gates, stance transition, clearance or full player lifecycle.')
(root/'artifacts/player-crouch-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

"""Original post-entity-update support routing versus PC/NXDK."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe');calls=[];falling=special=0

def hook(uc,address,size,context):
 if address not in (0x42a020,0x4895d0,0x4281a0,0x4a0840):return
 calls.append(address);sp=uc.reg_read(UC_X86_REG_ESP);ret,actor=struct.unpack('<II',uc.mem_read(sp,8));assert actor==base
 if address==0x42a020:uc.reg_write(UC_X86_REG_EAX,falling)
 elif address==0x4895d0:uc.reg_write(UC_X86_REG_EAX,special)
 uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
entry=int(re.search(r'\s_rf_entity_support_route\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=[];expected=[];counts=[0,0,0];special_calls=0
for flags,falling,mode,linked,moved,body,special in itertools.product((0,1,2,0x100),(0,1,255,256),(0,1,3,11),(-1,0,7),(0,1,256),(0,0x400000),(0,1,256)):
 raw=pack(flags,falling,mode,linked,moved,body,special);cases.append(raw)
 u.mem_write(base+0x810,pack(flags));u.mem_write(base+0x858,pack(base+0x2000));u.mem_write(base+0x2004,pack(mode));u.mem_write(base+0x200,pack(linked));u.mem_write(base+0x1a8,pack(body))
 u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,moved);u.mem_write(stack-4,pack(base));u.reg_write(UC_X86_REG_ESP,stack-4);calls=[]
 u.emu_start(0x487f6d,0x487fc9,count=100);assert u.reg_read(UC_X86_REG_EIP)==0x487fc9 and u.reg_read(UC_X86_REG_ESP)==stack
 route=2 if 0x4281a0 in calls else 1 if 0x4a0840 in calls else 0;counts[route]+=1;special_calls+=0x4895d0 in calls;expected.append(pack(route))
 nx.mem_write(base,raw);nx.mem_write(stack,pack(stop,base));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(entry,stop,count=100)
 assert nx.reg_read(UC_X86_REG_EIP)==stop and nx.reg_read(UC_X86_REG_EAX)==route,('NXDK',raw.hex(),route)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--support-route'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(cases),none=counts[0],query=counts[1],fall=counts[2],special_predicate_calls=special_calls,original_sha256=sha,scope='Prepared487f6d..487fc9 after entity update. Predicate returns supplied; original query/fall boundaries observed. PC/NXDK routing exact across flags, modes, links, moved byte and moving-support bit. Earlier moved/flag update, predicate implementation, list order, collision and live scheduler remain separate.')
(root/'artifacts/entity-support-route.json').write_text(json.dumps(report,indent=2));print(report)

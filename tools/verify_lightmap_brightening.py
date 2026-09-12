"""Original lightmap upload query initializer and capability decision."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
original=root/'Installed_Game/RF.exe';native=root/'build/xbox/main.exe'
assert hashlib.sha256(original.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(data)+4095)//4096*4096);u.mem_write(base,data);u.mem_map(b,65536);return u
def run(u,entry,args=b''):
 u.mem_write(stack,w(stop)+args);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 return u.reg_read(UC_X86_REG_EAX)
u=machine(original);x=machine(native);run(u,0x50c0e0)
mode=struct.unpack('<I',u.mem_read(0x17756c4,4))[0];fields=[(mode>>i)&31 for i in range(0,30,5)];assert fields==[5,2,3,3,4,0],fields
entry=int(re.search(r'\s_rf_lightmap_requires_brightening\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=[];results=[]
for renderer,multi,m2x in itertools.product([0,101,102,103,104,105,0xffffffff],[0,1,2,128,255,256,257],[0,1,2,128,255,256,257]):
 u.mem_write(0x17c7bcc,w(renderer));u.mem_write(0x1cfcc1c,bytes([multi&255,m2x&255]))
 supported=run(u,0x50df50,w(0x17756c4))&255;expected=int(not supported)
 command=w(renderer,multi,m2x);actual=run(x,entry,command);assert actual==expected,(renderer,multi,m2x,actual,expected)
 commands.append(command);results.append(w(expected))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_lightmap_probe.exe'),'--brightening'],input=b''.join(commands));assert actual==b''.join(results)
report=dict(result='PASS',cases=len(commands),query_mode=hex(mode),query_fields=fields,nxdk_sha256=hashlib.sha256(native.read_bytes()).hexdigest(),scope='Unhooked original50c0e0/411e00 initializer then50df50/546a00/helpers vs shared PC/NXDK brightening decision.343 renderer/byte-capability combinations, including noncanonical true values and high-byte noise. Upload query texture source5. Does not claim live capability selection for the port renderer, device initialization or visual parity.')
(root/'artifacts/lightmap-brightening-policy.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

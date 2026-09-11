"""Original prepared surface probe/reset gates versus PC and NXDK."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe')
def hook(uc,address,size,context):
 if address in (0x4a046e,0x4a073c,0x49fedf,0x49fee6):uc.emu_stop()
u.hook_add(UC_HOOK_CODE,hook)
maptext=(root/'build/xbox/main.map').read_text();entries=[int(re.search(r'\s_rf_physics_surface_'+name+r'_gate\s+([0-9a-fA-F]+)',maptext)[1],16) for name in ('probe','reset')]
fields=[0,0x80000000,0x3f599999,0x3f59999a,0x3f59999b,0x3f800000,0xbf800000,1,0x80000001,0x7f800000,0xff800000,0x7fc00001,0x7f800001,0xffc00123]
cases=[];expected=[];actions=[0,0];changed=[0,0]
for phase,bits,flags,surface in itertools.product((0,1),fields,(0,0x4000,0x08000000,0x10000000,0x18000000,0xffffffff),(0xffffffff,0xfffffffe,0,1,9,0x7fffffff)):
 raw=pack(phase,bits,flags,surface);cases.append(raw)
 u.mem_write(base+0x10c,pack(bits));u.mem_write(base+0x1b0,pack(bits));u.mem_write(base+0x1a8,pack(flags));u.mem_write(base+0x1380,pack(surface))
 u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x49feb6 if phase else 0x4a0406,stop,count=100)
 ip=u.reg_read(UC_X86_REG_EIP);assert ip in ((0x49fedf,0x49fee6) if phase else (0x4a046e,0x4a073c))
 action=int(ip==(0x49fedf if phase else 0x4a046e));want=pack(action)+bytes(u.mem_read(base+0x1380,4));expected.append(want);actions[phase]+=action;changed[phase]+=want[4:]!=pack(surface)
 nx.mem_write(base,raw);nx.mem_write(stack,pack(stop,bits,flags,base+12));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(entries[phase],stop,count=1000)
 assert nx.reg_read(UC_X86_REG_EIP)==stop
 actual=pack(nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(base+12,4));assert actual==want,('NXDK',len(cases)-1,raw.hex(),actual.hex(),want.hex())
pc=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--surface-gates'],input=b''.join(cases))
assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(cases),probe_requests=actions[0],reset_requests=actions[1],surface_changes=changed,original_sha256=sha,scope='Prepared4a0406 entry gates to probe4a046e or cleanup4a073c; prepared49feb6 comparison/clear within the earlier flag4000 branch. Signed-zero, .85 neighbors, infinities and quiet/signaling NaN cases. PC/NXDK action and material writes exact. Full caller phase, contacts, material assignment and live NPC ownership remain open.')
(root/'artifacts/surface-gates.json').write_text(json.dumps(report,indent=2));print(report)

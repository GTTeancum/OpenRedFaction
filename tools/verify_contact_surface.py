"""Original427550 surface-route gates, with real numeric and class callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_contact_surface_route\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
route=None
def stop(cpu,at,size,data):
 global route
 route={0x42764d:2,0x42772b:3,0x427802:1,0x42780a:0}[at];cpu.emu_stop()
for at in (0x42764d,0x42772b,0x427802,0x42780a):u.hook_add(UC_HOOK_CODE,stop,begin=at,end=at)
def original(blob):
 global route
 flags,kind,material=struct.unpack('<III',blob[:12]);u.mem_write(B,bytes(0x5000));u.mem_write(B+0x294,w(B+0x3000));u.mem_write(B+0x3724,w(flags));u.mem_write(B+0x31b4,w(kind));u.mem_write(B+0x1d0,w(material));u.mem_write(B+0x8c0,blob[12:16]);u.mem_write(B+0x144,blob[16:28]);u.mem_write(B+0x60,blob[28:40]);u.mem_write(B+0x1c0,blob[40:52]);u.mem_write(STACK,w(STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,B);u.reg_write(UC_X86_REG_FPCW,0x37f);route=None
 u.emu_start(0x427550,STOP,count=10000);assert route is not None;return w(0,route)
def compiled(blob):
 x.mem_write(B,blob);x.mem_write(B+0x100,w(0xa5a5a5a5));x.mem_write(STACK,w(STOP,B,B+0x100));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x100,4))
rng=random.Random(0x4275d2);cases=[]
for n in range(4096):
 flags=[0,0x200,0x400,0x600][n%4];kind=(n//4)%4;material=[0,8,9][(n//16)%3]
 vals=[rng.uniform(-8,8) for _ in range(10)];cases.append(w(flags,kind,material)+f(*vals))
# Exact, adjacent-float and lazy-field boundaries, including combined-flag precedence.
for flags in (0,0x200,0x400,0x600):
 for kind in (0,1,2):
  for bits in (0xc0400001,0xc0400000,0xc03fffff,0xbf666667,0xbf666666,0xbf666665):
   dot=struct.unpack('<f',w(bits))[0]
   cases.append(w(flags,kind,8)+f(0,1,0,0,1,0,0,dot,0,0))
# Equal zero speed/velocity gate must not enter APC even with a head-on normal.
for flags in (0x200,0x600):cases.append(w(flags,1,8)+f(0,0,0,0,1,0,0,-4,0,0))
expected=[];routes={}
for n,blob in enumerate(cases):
 result=original(blob);actual=compiled(blob);assert actual==result,(n,blob.hex(),actual.hex(),result.hex());expected.append(result);r=struct.unpack('<II',result)[1];routes[r]=routes.get(r,0)+1
count=len(cases)
# Reached nonfinite inputs fail without publishing a route.
for offset,flags in ((12,0x200),(16,0x200),(28,0x200),(40,0x200),(16,0x400),(40,0x400)):
 blob=bytearray(w(flags,1,8)+f(0,4,0,0,1,0,0,-4,0,0));blob[offset:offset+4]=w(0x7fc00000);blob=bytes(blob);result=compiled(blob);assert result==w(0xfffffffc,0xa5a5a5a5);cases.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--contact-surface'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',original_pc_nxdk_cases=count,nonfinite_guards=6,routes=routes,scope='Original427550 surface entry with actual40cac0,40a270,429990,486c90,40a180 and40a0b0. Stop at APC effects42764d, Driller effects42772b, sound call427802 or return42780a. Exact branch selection including strict thresholds and APC precedence. Effect bodies, object/timed branch and live scheduling excluded.')
(root/'artifacts/contact-surface.json').write_text(json.dumps(report,indent=2));print(report)

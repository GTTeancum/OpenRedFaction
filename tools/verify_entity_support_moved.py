"""Original support movement flags and unmodified squared-distance callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe');rng=random.Random(0x487f20);cases=[];expected=[];moved_count=0
entry=int(re.search(r'\s_rf_entity_support_moved\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
fixtures=[(0,0,0,0,0,0),(0x80000000,0,0,0,0,0),(1,0,0,0,0,0),(0x7f800000,0,0,0x7f800000,0,0),(0x7fc00001,0,0,0,0,0),(0x7f7fffff,0,0,0xff7fffff,0,0)]
for k in range(1600):
 flags=(rng.getrandbits(32)&~0x06000000)|((k%4)<<25)
 vectors=fixtures[(k//4)%len(fixtures)] if k<240 else tuple(rng.getrandbits(32) for _ in range(6))
 raw=pack(flags,*vectors);cases.append(raw)
 u.mem_write(base+0x7c,raw[:4]);u.mem_write(base+0x6c,raw[4:16]);u.mem_write(base+0xe4,raw[16:28]);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x487f20,0x487f67,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x487f67 and u.reg_read(UC_X86_REG_ESP)==stack
 moved=u.reg_read(UC_X86_REG_EBX)&255;assert moved in (0,1);moved_count+=moved
 want=pack(moved)+bytes(u.mem_read(base+0x7c,4));expected.append(want)
 assert bytes(u.mem_read(base+0x6c,12))+bytes(u.mem_read(base+0xe4,12))==raw[4:]
 nx.mem_write(base,raw);nx.mem_write(stack,pack(stop,base,base+4,base+16));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(entry,stop,count=1000)
 assert nx.reg_read(UC_X86_REG_EIP)==stop
 got=pack(nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(base,4));assert got==want,('NXDK',k,raw.hex(),got.hex(),want.hex())
 assert bytes(nx.mem_read(base+4,24))==raw[4:]
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--support-moved'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(cases),moved=moved_count,original_sha256=sha,scope='Original487f20..487f67 with actual4faf00/409fa0/40a180; no hooks. PC/NXDK moved byte and object flags exact, coordinates preserved. Includes flags bypass, equal/signed-zero points, subnormals, overflow, infinities and NaNs. Earlier position-history ownership and later entity/support phases remain external.')
(root/'artifacts/entity-support-moved.json').write_text(json.dumps(report,indent=2));print(report)

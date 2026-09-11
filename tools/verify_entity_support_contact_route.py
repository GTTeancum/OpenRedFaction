"""Original post-query contact routing with real lookup and movement predicates."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
import itertools
u=machine(exe);nx=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_support_contact_route\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
ends={0x4a0ad1:1,0x4a0c82:1,0x4a0c8a:0,0x4a0b31:2,0x4a0ae3:3}
def stop_route(m,address,size,data):
 if address in ends:m.emu_stop()
u.hook_add(UC_HOOK_CODE,stop_route)
cases=[];expected=[];counts=[0]*4
bits=[0,0xbf800000,0x3effffff,0x3f000000,0x3f7fffff,0x3f800000,0x7f800000,0x7fc01234]
for frac,dot,lookup,typ,high,mode in itertools.product(bits,bits,range(3),(0,3),(0,1),(1,3,8)):
 resolved=int(lookup==1);flags=high<<31;falling=int(mode in (3,8));raw=pack(frac,dot,resolved,typ,flags,falling);cases.append(raw)
 u.mem_write(base,bytes(0x7000))
 for off,value in [(0x24,0),(0x294,base+0x4000),(0x858,base+0x6000),(0x1380,0),(0x3024,typ),(0x302c,0x10002),(0x31a8,flags),(0x6004,mode)]:u.mem_write(base+off,pack(value))
 u.mem_write(0x7394d4,pack(base+0x3000));u.mem_write(0x7c6ec8,pack(0,0x3f800000,0))
 u.mem_write(stack,bytes(0x100));u.mem_write(stack+0x60,pack(frac));u.mem_write(stack+0x54,pack(0,dot,0));u.mem_write(stack+0x78,pack([0xffffffff,0x10002,0x20002][lookup]))
 u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack-16)
 u.emu_start(0x4a0a5c,0x4a0c94,count=10000);ip=u.reg_read(UC_X86_REG_EIP);assert ip in ends,hex(ip)
 want=pack(ends[ip]);expected.append(want);counts[ends[ip]]+=1
 # The original dot with(0,1,0) matches this widened Y for these fixtures.
 dot_value=struct.unpack('<f',pack(dot))[0]
 nx.mem_write(stack,pack(stop,frac)+struct.pack('<d',dot_value)+pack(resolved,typ,flags,falling));nx.reg_write(UC_X86_REG_ESP,stack)
 nx.emu_start(entry,stop,count=1000);assert nx.reg_read(UC_X86_REG_EIP)==stop
 assert pack(nx.reg_read(UC_X86_REG_EAX))==want,('NXDK',raw.hex(),ends[ip])
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--support-contact-route'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(cases),routes=counts,original_sha256=sha,scope='Original4a0a5c through static/moving/fall/return boundaries. Real40a0b0 dot,40a0e0 missing/valid/stale handle lookup,4136d0 and42a020 movement predicates; hooks only observe final decisions. Modes1/3/8, type0/3, body high bit, fraction/normal thresholds including infinity/NaN. PC/NXDK resolved-route helper exact. Query, contact mutation and fall/landing execution excluded; arbitrary dot rounding is caller-owned.')
(root/'artifacts/entity-support-contact-route.json').write_text(json.dumps(report,indent=2));print(report)

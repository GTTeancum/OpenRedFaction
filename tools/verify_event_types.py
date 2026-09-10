"""Execute original case-insensitive type lookup and compare PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*[v&0xffffffff for v in v])
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
names=[]
for a in range(0x5a1a3c,0x5a1ba4,4):
 p=struct.unpack('<I',u.mem_read(a,4))[0];names.append(bytes(u.mem_read(p,64)).split(b'\0')[0])
cases=[]
for name in names:cases.extend((name,name.lower(),name.upper(),name+b'_',b' '+name,name[:-1]))
cases.extend((b'',b'Set_Gravity\xff',b'not_an_event',b'x'*255))
commands=bytearray();expected=bytearray()
for name in cases:
 u.mem_write(base,w(len(name),base+0x100));u.mem_write(base+0x100,name+b'\0');u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4bd700,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 value=u.reg_read(UC_X86_REG_EAX);wanted=next((i for i,n in enumerate(names) if n.lower()==name.lower()),0xffffffff);assert value==wanted
 commands.extend(name.ljust(256,b'\0'));expected.extend(w(value))
probe=root/'build/pc/Release/rf_event_probe.exe';assert subprocess.check_output([str(probe),'--type-id'],input=commands)==expected
entry=int(re.search(r'_rf_event_type_id\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,name in enumerate(cases):
 x.mem_write(base,name+b'\0');x.mem_write(stack,w(stop,base));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
 assert w(x.reg_read(UC_X86_REG_EAX))==expected[i*4:i*4+4]
report=dict(result='PASS',types=90,cases=len(cases),original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Actual 4bd700 lookup and string comparison execute unchanged. Canonical/case variants, prefixes, suffixes and invalid names; shared PC/NXDK type IDs only, not action implementations.')
(root/'artifacts/event-type-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

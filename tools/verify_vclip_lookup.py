"""Original fixed-slot vclip lookup vs PC/NXDK with authored name fixtures."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
def words(*v):return struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
entry=int(re.search(r'_rf_vclip_name_lookup\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inventory=json.loads((root/'artifacts/inventory.json').read_text())
e=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='vclip.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);data=f.read(e['size'])
names=re.findall(r'(?im)^\s*\$Name:\s*"([^"]*)"',data.decode('cp1252'));assert len(names)<=64 and len(set(names))==len(names)
commands=bytearray();expected=bytearray();results=[]
fixtures=[(names+['']*(64-len(names)),q) for n in names for q in (n,n.lower(),n.upper(),n+'?',n+' ',n[:-1])]
fixtures += [(names+['']*(64-len(names)),q) for q in ('','missing_vclip')]
fixtures += [(['']*63+['last'],'LAST'),(['dup','DUP']+['']*62,'dup'),(['']*64,'')]
for slots,query in fixtures:
 payload=b''.join(s.encode('cp1252').ljust(128,b'\0') for s in slots+[query]);assert len(payload)==65*128
 commands.extend(payload);addresses=[base+0x1000+i*128 for i in range(64)];qaddr=base+0x1000+64*128
 for cpu in (u,x):cpu.mem_write(base+0x1000,payload)
 for i,name in enumerate(slots):u.mem_write(0x858cb8+i*224,words(len(name),addresses[i]))
 x.mem_write(base,words(*addresses));got=[]
 for cpu,native in ((u,False),(x,True)):
  cpu.mem_write(stack,words(stop,base,qaddr) if native else words(stop,qaddr));cpu.reg_write(UC_X86_REG_ESP,stack)
  cpu.emu_start(entry if native else 0x4c1d00,stop,count=100000)
  assert cpu.reg_read(UC_X86_REG_EIP)==stop and cpu.reg_read(UC_X86_REG_ESP)==stack+4
  got.append(cpu.reg_read(UC_X86_REG_EAX))
 want=next((i for i,n in enumerate(slots) if query and n.lower()==query.lower()),0xffffffff)
 assert got==[want,want],(query,got,want)
 expected.extend(words(want));results.append(dict(query=query,index=want))
probe=root/'build/pc/Release/rf_effect_probe.exe';assert subprocess.check_output([str(probe),'--vclip-lookup'],input=commands)==expected
report=dict(result='PASS',cases=len(results),authored_names=len(names),original_sha256=sha,table_sha256=hashlib.sha256(data).hexdigest(),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Unchanged original 4c1d00/string accessor/CRT comparison with synthetic 64-slot table; exact ASCII name results match PC/NXDK, including duplicate first match and final slot. Authored names extracted for fixtures; original table loader and non-ASCII locale behavior excluded.',results=results)
(root/'artifacts/vclip-lookup-verification.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})

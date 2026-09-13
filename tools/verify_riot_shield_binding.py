"""Original40f360 global shield binding from authored clutter names."""
import hashlib,json,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
j=json.loads((root/'artifacts/inventory.json').read_text());e=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='clutter.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
names=re.findall(r'(?im)^\s*\$Class\s+Name:\s*"([^"]+)"',text)
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,0x400000);u.mem_write(0x5c97dc,struct.pack('<I',len(names)))
for i,name in enumerate(names):
 b=name.encode('cp1252');assert len(b)<256;u.mem_write(B+i*256,b+b'\0');u.mem_write(0x5afb88+i*232,struct.pack('<II',len(b),B+i*256))
u.reg_write(UC_X86_REG_ESP,B+0x3ff000);u.emu_start(0x40f360,0x40f36f,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==0x40f36f
index=struct.unpack('<I',bytes(u.mem_read(0x5afb78,4)))[0];assert index==262 and names[index].lower()=='riot_shield'
report=dict(result='PASS',classes=len(names),index=index,scope='Original40f360..40f36f with real410b60/string lookup and authored class-name storage. Full class parser and runtime creation excluded.')
(root/'artifacts/riot-shield-binding.json').write_text(json.dumps(report,indent=2));print(report)

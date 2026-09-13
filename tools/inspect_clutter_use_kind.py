"""Original use-name classification and installed clutter attachment requirements."""
import hashlib,json,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(raw)+4095)&~4095);u.mem_write(0x400000,raw)
base=0x30000000;stack=base+0xe000;u.mem_map(base,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def boundary(m,a,s,d):m.emu_stop()
for a in (0x48977c,0x4897a1):u.hook_add(UC_HOOK_CODE,boundary,begin=a,end=a)
def classify(name):
 data=name.encode('ascii');u.mem_write(base,data+b'\0');u.mem_write(base+1024,w(0xa5a5a5a5));u.mem_write(stack+4,w(len(data),base));u.mem_write(stack+0x20,w(base+1024));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x489662,0x4897a1,count=100000);assert u.reg_read(UC_X86_REG_EIP) in (0x48977c,0x4897a1)
 return struct.unpack('<I',u.mem_read(base+1024,4))[0]
kinds=dict(vehicle=1,switch=2,command=3,turret=4,monitor=5,medic=6,**{'ai response':9,'play_sound':10})
for name,value in kinds.items():assert classify(name)==value,(name,classify(name),value)
assert classify('unknown')==0
case_results={name:classify(name.upper()) for name in kinds}
inv=json.loads((root/'artifacts/inventory.json').read_text());entry=next(e for a in inv['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='clutter.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);table=f.read(entry['size'])
clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],table.decode('cp1252'))
markers=list(re.finditer(r'(?im)^\s*\$Class Name:\s*"([^"]+)"',clean));rows=[]
for i,m in enumerate(markers):
 block=clean[m.end():markers[i+1].start() if i+1<len(markers) else len(clean)];block=re.split(r'(?im)^\s*\$Skin:',block)[0]
 use=re.search(r'(?im)^\s*\$Use:\s*"([^"]*)"',block)
 if use:
  rows.append(dict(name=m[1],use=use[1],kind=classify(use[1]),glares=re.findall(r'(?im)^\s*\$(?:Glare|Rod Glare):\s*"([^"]*)"',block)))
report=dict(result='PASS',original_sha256=sha,class_count=len(markers),use_classes=rows,uppercase_results=case_results,
 scope='Original489662..4897a1 use classifier with actual5001d0 comparison and prepared original string. Parser/string lifetime/radius parsing excluded. Installed first-class blocks before skins inspected, not live placement. Clutter class base5afb88 plus44 is passed to489610 at40f9ba; use4 means turret and selects alternate attachment matrix7e0.')
(root/'artifacts/clutter-use-kind.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


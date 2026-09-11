"""Original extension extraction/classification against PC and NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,0x20000);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_entity_model_kind\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=['','miner.vcm','effect.vfx','mesh.v3d','mesh.v3c','noextension','a.vcm.b','dir.vfx/name','a.','a.vfx/child','x'*63]
for suffix in ('vcm','vfx'):
 for mask in range(8):cases.append('name.'+''.join(c.upper() if mask&(1<<i) else c for i,c in enumerate(suffix)))
results=[]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
record=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as file:file.seek(record['offset']);table=file.read(record['size'])
text='\n'.join(line.split('//',1)[0] for line in table.decode('cp1252').splitlines())
cases+=sorted(set(re.findall(r'\$V3D\s+Filename:\s*"([^"\r\n]*)"',text)))
for name in cases:
 raw=name.encode()+b'\0';u.mem_write(b+0x4000,raw);u.mem_write(stack,w(stop,b+0x4000));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x5143f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 extension=u.reg_read(UC_X86_REG_EAX);s=bytes(u.mem_read(extension,64)).split(b'\0')[0]
 # Supply only the temporary rf_string wrapper; original extraction and actual
 # string comparison callees execute without replacement.
 u.mem_write(b,bytes([0xa5])*0x1514);u.mem_write(stack+0x10,w(len(s),extension));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,b)
 u.emu_start(0x41ba4d,0x41ba9b,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41ba9b
 wanted=bytes(u.mem_read(b+0x94,4));before=bytearray([0xa5]*0x1514);before[0x94:0x98]=wanted;assert bytes(u.mem_read(b,len(before)))==before
 pc=list(map(int,subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--model-kind',name]).split()))
 assert pc==[0,struct.unpack('<I',wanted)[0]],(name,pc,wanted)
 x.mem_write(b+0x4000,raw);x.mem_write(b,w(0xa5a5a5a5));x.mem_write(stack,w(stop,b+0x4000,b));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b,4))==wanted
 results.append(dict(model=name,kind=pc[1]))
assert len(cases)>27,'Installed model declarations missing'
pc=list(map(int,subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--model-kind','x'*64]).split()))
assert pc[0]!=0 and pc[1]==0xa5a5a5a5
x.mem_write(b+0x4000,b'x'*64+b'\0');x.mem_write(b,w(0xa5a5a5a5));x.mem_write(stack,w(stop,b+0x4000,b));x.reg_write(UC_X86_REG_ESP,stack)
x.emu_start(entry,stop,count=100000)
assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(b,4))==w(0xa5a5a5a5)
report=dict(result='PASS',cases=len(results),original_sha256=digest,results=results,scope='Original5143f0 and41ba4d..41ba9b with actual string callees; only temporary string wrapper supplied. PC/NXDK classifier exact on ASCII fixtures and installed model declarations. No full model loading or actor construction claim.')
(root/'artifacts/entity-model-kind.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})

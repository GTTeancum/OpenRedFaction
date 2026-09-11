"""Independent installed action declarations versus PC/NXDK parser."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
parts=re.split(r'\$Name:\s*"([^"\r\n]+)"',text);rows=[]
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,0x200000)
function=int(re.search(r'_rf_entity_action_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
for name,body in zip(parts[1::2],parts[2::2]):
 block=('$Name: "'+name+'"'+body).encode('cp1252');x.mem_write(b+0x10000,block);weapon=''
 for match in re.finditer(r'\+Weapon\s+Specific:\s*"([^"\r\n]*)"|\+Action:\s*"([^"\r\n]*)"\s*"([^"\r\n]*)"\s*"([^"\r\n]*)"',body):
  if match[1] is not None:weapon=match[1];continue
  action,motion,sound=match[2],match[3],match[4]
  expected=w(0)+motion.encode().ljust(64,b'\0')+sound.encode().ljust(64,b'\0');assert len(expected)==132
  pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--action',str(root/'Installed_Game/tables.vpp'),name.upper(),weapon.upper(),action.upper()])
  assert pc==expected,('PC',name,weapon,action,pc[:4])
  for off,value in ((0x1000,name),(0x2000,weapon),(0x3000,action)):x.mem_write(b+off,value.upper().encode()+b'\0')
  x.mem_write(b,bytes([0xa5])*128);x.mem_write(stack,w(stop,b+0x10000,len(block),b+0x1000,b+0x2000,b+0x3000,b));x.reg_write(UC_X86_REG_ESP,stack)
  x.emu_start(function,stop,count=100000000)
  assert x.reg_read(UC_X86_REG_EIP)==stop and w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,128))==expected,('NXDK',name,weapon,action)
  rows.append(dict(entity_class=name,weapon=weapon,action=action,motion=motion,sound=sound))
guards=['$Name: "x" +Action: "a" "clip"',
 '$Name: "x" +Action: "a" "clip" "" +Action: "a" "clip2" ""',
 '$Name: "x" +Weapon Specific: "rifle" +Action: "a" "clip" ""',
 '$Name: "other" +Action: "a" "clip" ""',
 '$Name: "x" +Action: "a" "'+('x'*64)+'" ""']
for fixture in guards:
 data=fixture.encode();pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--action-text','x','','a'],input=data)
 assert pc[:4]!=w(0) and pc[4:]==bytes([0xa5])*128
 x.mem_write(b+0x10000,data)
 for off,value in ((0x1000,'x'),(0x2000,''),(0x3000,'a')):x.mem_write(b+off,value.encode()+b'\0')
 x.mem_write(b,bytes([0xa5])*128);x.mem_write(stack,w(stop,b+0x10000,len(data),b+0x1000,b+0x2000,b+0x3000,b));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(function,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,128))==pc
report=dict(result='PASS',declarations=len(rows),guards=len(guards),table_sha256=hashlib.sha256(raw).hexdigest(),rows=rows,scope='Independent regex extraction of installed +Action triples compared with PC full-table reader and NXDK class-block reader; exact base/weapon selection with uppercase queries. Port parser verification, not full original table loader or sound-ID/registration equivalence.')
(root/'artifacts/entity-actions.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='rows'})

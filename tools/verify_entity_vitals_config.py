"""Installed class health/armor/FOV through original assignment and port loaders."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xf000;stub=b+0xf100
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda v:struct.pack('<f',v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,1024*1024);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_entity_vitals_config_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.mem_write(stub,b'\xd9\x05'+w(stub+16)+b'\xc3')
values=[];tokens=[];reads=0
def supplied(m,a,size,data):
 global reads
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if a==0x5126a0:
  tokens.append(struct.unpack('<I',m.mem_read(sp+4,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+8);m.reg_write(UC_X86_REG_EIP,ret)
 else:
  m.mem_write(stub+16,f(values[reads]));reads+=1;m.reg_write(UC_X86_REG_EIP,stub)
for address in (0x5126a0,0x512920):u.hook_add(UC_HOOK_CODE,supplied,begin=address,end=address)
inventory=json.loads((root/'artifacts/inventory.json').read_text())
record=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as file:file.seek(record['offset']);raw=file.read(record['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
parts=re.split(r'\$Name:\s*"([^"\r\n]+)"',text);assert len(parts)==127
x.mem_write(b+0x10000,raw);results=[]
for name,block in zip(parts[1::2],parts[2::2]):
 fields=[]
 for key in ('FOV','Envirosuit','Life'):
  matches=re.findall(r'\$'+key+r':\s*([^\s]+)',block);assert len(matches)==1,(name,key)
  fields.append(float(matches[0]))
 values=fields;tokens=[];reads=0;u.mem_write(b,bytes([0xa5])*0x1514);u.mem_write(stack,w(stop))
 u.reg_write(UC_X86_REG_EBP,b);u.reg_write(UC_X86_REG_EBX,b+0x8000);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x41bcba,0x41bd0d,count=10000)
 assert tokens==[0x595050,0x595058,0x595068] and reads==3
 want=bytes(u.mem_read(b+0x44,8))+bytes(u.mem_read(b+0x764,4))
 before=bytearray([0xa5]*0x1514);before[0x44:0x4c]=want[:8];before[0x764:0x768]=want[8:]
 assert bytes(u.mem_read(b,len(before)))==before
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vitals-config',str(root/'Installed_Game/tables.vpp'),name])
 assert pc==w(0)+want,('PC',name,pc.hex(),want.hex())
 x.mem_write(b+0x8000,name.encode()+b'\0');x.mem_write(b+0x9000,bytes([0xa5])*12);x.mem_write(stack,w(stop,b+0x10000,len(raw),b+0x8000,b+0x9000))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b+0x9000,12))==want,('NXDK',name,fields,x.reg_read(UC_X86_REG_EAX),bytes(x.mem_read(b+0x9000,12)).hex(),want.hex())
 results.append(dict(name=name,fov=fields[0],health=fields[2],armor=fields[1],half_fov_cosine_bits=struct.unpack('<I',want[8:])[0]))
# Port malformed-input guards preserve all output bytes, including late errors.
fixtures=['$Name: "x" $FOV: 90 $Life: 100', '$Name: "x" $FOV: 361 $Life: 100 $Envirosuit: 2',
 '$Name: "x" $FOV: 90 $Life: 100 $Life: 200 $Envirosuit: 2', '$Name: "x" $FOV: nan $Life: 100 $Envirosuit: 2',
 '$Name: "other" $FOV: 90 $Life: 100 $Envirosuit: 2']
for text in fixtures:
 raw_fixture=text.encode();pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vitals-text','x'],input=raw_fixture)
 assert pc[:4]!=w(0) and pc[4:]==bytes([0xa5])*12
 x.mem_write(b+0x10000,raw_fixture);x.mem_write(b+0x8000,b'x\0');x.mem_write(b+0x9000,bytes([0xa5])*12);x.mem_write(stack,w(stop,b+0x10000,len(raw_fixture),b+0x8000,b+0x9000))
 x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x9000,12))==pc
report=dict(result='PASS',classes=len(results),guards=len(fixtures),original_sha256=digest,table_sha256=hashlib.sha256(raw).hexdigest(),results=results,
 scope='All installed63 classes: original41bcba..41bd0d token/float boundaries supplied, actual x87 FOV conversion/stores; PC archive loader and NXDK text reader match exact class fields. Required token order observed. Malformed port inputs preserve output. Full original text parser, entity creation and live NPC ownership excluded.')
(root/'artifacts/entity-vitals-config.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})

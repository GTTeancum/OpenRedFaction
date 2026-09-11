"""Class damage factors: original assignment/name lookup vs PC and NXDK reader."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=0x30000000;stack=b+0xe000;stop=b+0xf000;stub=b+0xf100
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(im)+4095)//4096*4096);u.mem_write(origin,im);u.mem_map(b,1024*1024);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_damage_factors_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.mem_write(stub,b'\xd9\x05'+w(stub+16)+b'\xc3')
values=[];at=0
def supplied(m,a,size,data):
 global at
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if a==0x5125c0:
  m.reg_write(UC_X86_REG_EAX,int(at<len(values)));m.reg_write(UC_X86_REG_ESP,sp+8);m.reg_write(UC_X86_REG_EIP,ret)
 elif a==0x512bb0:
  dest=struct.unpack('<I',m.mem_read(sp+4,4))[0];m.mem_write(b+0x8000,values[at][0].encode()+b'\0');m.mem_write(dest+4,w(b+0x8000))
  m.reg_write(UC_X86_REG_ESP,sp+16);m.reg_write(UC_X86_REG_EIP,ret)
 else:
  m.mem_write(stub+16,struct.pack('<f',float(values[at][1])));at+=1;m.reg_write(UC_X86_REG_EIP,stub)
for a in (0x5125c0,0x512bb0,0x512920):u.hook_add(UC_HOOK_CODE,supplied,begin=a,end=a)
inv=json.loads((root/'artifacts/inventory.json').read_text())
r=next(e for a in inv['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(r['offset']);raw=f.read(r['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
parts=re.split(r'\$Name:\s*"([^"\r\n]+)"',text);assert len(parts)==127
cases=[(name,raw,block) for name,block in zip(parts[1::2],parts[2::2])]
for block in ['', '$Damage Type Factor: "BULLET" 2\n$Damage Type Factor: "bullet" .5']:
 data=('$Name: "fixture"\n'+block).encode();cases.append(('fixture',data,block))
report=[];path=root/'artifacts/damage-factors-input.tbl'
for name,data,block in cases:
 values=re.findall(r'\$Damage\s+Type\s+Factor:\s*"([^"\r\n]*)"\s*([^\s]+)',block,re.I);at=0
 u.mem_write(b,b'\xa5'*0x1514);u.mem_write(stack,bytes(0x200));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,b);u.reg_write(UC_X86_REG_EBX,b+0x9000);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x41bd0d,0x41bd7c,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==0x41bd7c and at==len(values)
 expected=bytes(u.mem_read(b+0x13e8,44));guard=bytearray(b'\xa5'*0x1514);guard[0x13e8:0x1414]=expected;assert bytes(u.mem_read(b,0x1514))==guard
 path.write_bytes(data);pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--damage-factors',str(path),name]);assert pc==w(0)+expected,('PC',name)
 x.mem_write(b+0x10000,data);x.mem_write(b+0x8000,name.encode()+b'\0');x.mem_write(b+0x9000,b'\xa5'*44);x.mem_write(stack,w(stop,b+0x10000,len(data),b+0x8000,b+0x9000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b+0x9000,44))==expected,('NXDK',name)
 report.append(dict(name=name,overrides=len(values),factors=list(struct.unpack('<11f',expected))))
guards=['$Damage Type Factor: "unknown" 1','$Damage Type Factor: "" 1',
 '$Damage Type Factor: "fire" nan','$Damage Type Factor: "bullet" 1e999',
 '$Damage Type Factor: "fire"','$Damage Type Factor: fire 1']
for block in guards:
 data=('$Name: "fixture"\n'+block).encode();path.write_bytes(data)
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--damage-factors',str(path),'fixture'])
 assert struct.unpack('<i',pc[:4])[0]!=0 and pc[4:]==b'\xa5'*44
 x.mem_write(b+0x10000,data);x.mem_write(b+0x8000,b'fixture\0');x.mem_write(b+0x9000,b'\xa5'*44)
 x.mem_write(stack,w(stop,b+0x10000,len(data),b+0x8000,b+0x9000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(b+0x9000,44))==b'\xa5'*44
result=dict(result='PASS',port_guards=len(guards),cases=len(cases),installed_classes=63,scope='Original41bd0d..41bd7c defaults and ordered stores with real48ab50/57c130 name lookup. Parser token/string/float boundaries supplied; not original text parsing. PC and NXDK readers compare all11 factors and preserved original record bytes. Includes case-insensitive duplicate overwrite; slots9/10 remain defaults (null original name pointers).',classes=report)
(root/'artifacts/entity-damage-factors.json').write_text(json.dumps(result,indent=2));print({k:v for k,v in result.items() if k!='classes'})

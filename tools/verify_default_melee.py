"""Original optional melee parser block and compiled default-weapon reader."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
word=lambda c,a:struct.unpack('<I',bytes(c.mem_read(a,4)))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase;c.mem_map(base,(len(im)+4095)//4096*4096);c.mem_write(base,im);c.mem_map(B,0x200000);return c
B=0x30000000;STACK=B+0x1fe000;STOP=B+0x1ff000
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_default_weapons_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
names=json.loads((root/'artifacts/weapon-names.json').read_text())['names'];rows=json.loads((root/'artifacts/entity-default-weapons.json').read_text())['rows'];inv=json.loads((root/'artifacts/inventory.json').read_text());e=next(e for a in inv['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'));parts=re.split(r'\$Name:\s*"([^"\r\n]+)"',text);blocks=dict(zip(parts[1::2],parts[2::2]))
namewire=b''.join(n.encode().ljust(64,b'\0') for n in names).ljust(4096,b'\0')+w(len(names),40);x.mem_write(B+0x9000,namewire);x.mem_write(B+0x10000,raw);u.mem_write(0x872448,w(len(names)))
for i,n in enumerate(names):
 u.mem_write(B+0x10000+i*256,n.encode()+b'\0');u.mem_write(0x85cd08+i*0x550,w(len(n),B+0x10000+i*256))
melee=None
def scanner(c,at,size,data):
 sp=c.reg_read(UC_X86_REG_ESP)
 if at==0x5125c0:
  assert word(c,sp+4)==0x5951e4;c.reg_write(UC_X86_REG_EAX,int(melee is not None));cleanup=4
 else:
  target=word(c,sp+4);assert word(c,sp+8)==34 and word(c,sp+12)==34;c.mem_write(B+0x5000,melee.encode()+b'\0');c.mem_write(target,w(len(melee),B+0x5000));cleanup=12
 c.reg_write(UC_X86_REG_EIP,word(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
for at in (0x5125c0,0x512bb0):u.hook_add(UC_HOOK_CODE,scanner,begin=at,end=at)
count=0
for row in rows:
 found=re.findall(r'\$Default Melee:\s*"([^"\r\n]*)"',blocks[row['entity_class']]);melee=found[0] if found else None
 u.mem_write(B,w(0)*64);u.reg_write(UC_X86_REG_EBP,B);u.reg_write(UC_X86_REG_ESI,0xffffffff);u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x41c028,0x41c073,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41c073;result=word(u,B+0x90);assert result==(row['melee']&0xffffffff)
 x.mem_write(B+0x8000,row['entity_class'].upper().encode()+b'\0');x.mem_write(B,b'\xa5'*12);x.mem_write(STACK,w(STOP,B+0x10000,len(raw),B+0x8000,B+0x9000,B));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=20000000);assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0;assert bytes(x.mem_read(B,12))==w(row['primary'],row['secondary'],row['melee']);count+=int(melee is not None)
report=dict(result='PASS',classes=len(rows),authored_melee=count,scope='Original41c028..41c073 optional field with actual4ff490/4ff480/4c81f0 and supplied scanner tokens; compiled NXDK full table reader compared with independently verified PC rows. Constructor grants and native scene ownership excluded.')
(root/'artifacts/default-melee.json').write_text(json.dumps(report,indent=2));print(report)

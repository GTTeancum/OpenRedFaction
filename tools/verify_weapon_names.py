"""Weapon ID sequencing and original name lookup versus PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,0x200000);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
address=lambda name:int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
def stack_probe(m,a,size,data):
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 m.reg_write(UC_X86_REG_ESP,sp+4-m.reg_read(UC_X86_REG_EAX));m.reg_write(UC_X86_REG_EIP,ret)
# NXDK stack-probe ABI on pre-mapped fixture stack; kernel stack growth excluded.
x.hook_add(UC_HOOK_CODE,stack_probe,begin=address('_chkstk'),end=address('_chkstk'))
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='weapons.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
sections=re.findall(r'#(?:Primary|Secondary) Weapons\s*(.*?)#End',text,re.S);assert len(sections)==2
groups=[re.findall(r'\$Name:\s*"([^"]*)"',s) for s in sections];names=groups[0]+groups[1]
pack=lambda ns:b''.join(n.encode().ljust(64,b'\0') for n in ns).ljust(4096,b'\0')+w(len(ns),min(len(ns),len(groups[0])))
expected=pack(names)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--weapon-names',str(root/'Installed_Game/tables.vpp')]);assert pc==w(0)+expected
x.mem_write(b+0x10000,raw);x.mem_write(stack,w(stop,b+0x10000,len(raw),b));x.reg_write(UC_X86_REG_ESP,stack)
x.emu_start(address('rf_weapon_names_read'),stop,count=100000000)
assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b,4104))==expected
readword=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def boundary(m,a,size,data):
 sp=m.reg_read(UC_X86_REG_ESP);ret=readword(m,sp);count=readword(m,0x872448)
 cleanup=0
 if a==0x5125c0:
  token=readword(m,sp+4);assert token in (0x5a34d0,0x5a34ec)
  m.reg_write(UC_X86_REG_EAX,int(count==(len(groups[0]) if token==0x5a34d0 else len(names))));cleanup=4
 elif a==0x5126a0:assert readword(m,sp+4)==0x5a34d8;cleanup=4
 else:
  assert a==(0x4c2b60 if count<len(groups[0]) else 0x4c4850)
  m.mem_write(0x85cd08+count*0x550,w(len(names[count]),b+0x20000+count*64))
 m.reg_write(UC_X86_REG_ESP,sp+4+cleanup);m.reg_write(UC_X86_REG_EIP,ret)
for a in (0x5125c0,0x5126a0,0x4c2b60,0x4c4850):u.hook_add(UC_HOOK_CODE,boundary,begin=a,end=a)
for i,name in enumerate(names):u.mem_write(b+0x20000+i*64,name.encode()+b'\0')
u.mem_write(0x872448,w(0));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c6843,0x4c68d9,count=100000)
assert u.reg_read(UC_X86_REG_EIP)==0x4c68d9 and readword(u,0x872448)==len(names) and readword(u,0x87211c)==len(groups[0])
queries=[(names,s) for name in names for s in (name,name.lower(),name.upper())]+[(names,''),(names,'missing weapon')]
queries += [(['dup','DUP'],'DuP'),(['first','dup','DUP'],'dup'),([''],'') ,([],''),([], 'missing'),(['x',''],'')]
for table,query in queries:
 expected=pack(table);u.mem_write(0x872448,w(len(table)))
 for i,name in enumerate(table):
  u.mem_write(b+0x20000+i*64,name.encode()+b'\0');u.mem_write(0x85cd08+i*0x550,w(len(name),b+0x20000+i*64))
 u.mem_write(b,query.encode()+b'\0');u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c81f0,stop,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop;want=w(u.reg_read(UC_X86_REG_EAX))
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--weapon-find',query],input=expected);assert pc==want
 x.mem_write(b,expected);x.mem_write(b+0x8000,query.encode()+b'\0');x.mem_write(stack,w(stop,b,b+0x8000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(address('rf_weapon_name_find'),stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop and w(x.reg_read(UC_X86_REG_EAX))==want
report=dict(result='PASS',weapons=len(names),primary=len(groups[0]),lookup_cases=len(queries),original_sha256=digest,names=names,scope='Original4c6843..4c68d9 sequencing with record parsers/end-token reads supplied; original4c81f0 and string callees execute unchanged. Installed PC/NXDK table names/order and lookup agree; NXDK stack probe supplied on mapped fixture stack. Full weapon stats parser/constructor excluded.')
(root/'artifacts/weapon-names.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='names'})

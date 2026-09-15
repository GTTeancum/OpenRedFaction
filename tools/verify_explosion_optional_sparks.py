"""Original optional-sparks lookup/store/gate; source names supplied from installed table."""
import hashlib,json,re,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=R/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
j=json.loads((R/'artifacts/inventory.json').read_text());archive=(R/'Installed_Game/tables.vpp').read_bytes();entry=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='emitters.tbl');table=archive[entry['offset']:entry['offset']+entry['size']];names=re.findall(rb'(?im)^\s*\$name:\s*"([^"]*)"',re.sub(rb'//[^\n]*',b'',table))
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,65536);stack=B+0xe000;w=lambda *v:struct.pack('<'+'I'*len(v),*v)
u.mem_write(0x7bd99c,w(len(names)))
for i,name in enumerate(names):u.mem_write(B+i*128,name+b'\0');u.mem_write(0x7b2870+i*8,w(len(name),B+i*128))
rows=[]
for query in [b'explosion random bits 2',b'explosion random bits',b'EXPLOSION RANDOM BITS',b'explosion random bits 2 ']:
 u.mem_write(stack+0x10,query+b'\0');u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,B+0x8000)
 u.emu_start(0x48df58,0x48df6d,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==0x48df6d
 value=struct.unpack('<i',u.mem_read(B+0x8080,4))[0];expect=next((i for i,n in enumerate(names) if n.lower()==query.lower()),-1);assert value==expect
 reached=[]
 def hook(cpu,address,size,_):
  if address in (0x48e9b7,0x48ea8c):reached.append(address);cpu.emu_stop()
 h=u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_EBP,B+0x8000);u.emu_start(0x48e9a8,0x48ea8d,count=1000);u.hook_del(h)
 assert reached==([0x48ea8c] if value<0 else [0x48e9b7])
 rows.append(dict(query=query.decode(),resolved=value,sparks_path_entered=value>=0,stop=hex(reached[0])))
(R/'artifacts/crater-shading-re/optional-sparks.json').write_text(json.dumps(dict(exe_sha256=sha,table_sha256=hashlib.sha256(table).hexdigest(),names=len(names),scope=__doc__,rows=rows),indent=2));print(json.dumps(rows,indent=2))

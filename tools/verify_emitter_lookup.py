"""Original emitter-name lookup against installed names and recipe references."""
import json,re,struct,hashlib,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
j=json.loads((root/'artifacts/inventory.json').read_text());archive=(root/'Installed_Game/tables.vpp').read_bytes()
def table(n):
 e=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==n);return archive[e['offset']:e['offset']+e['size']]
names=re.findall(rb'(?im)^\s*\$name:\s*"([^"]*)"',table('emitters.tbl'));refs=re.findall(rb'(?im)^\s*[+$](?:Central|Sparks|Trail_Head|Trail_Tail)_Emitter:\s*"([^"]*)"',table('explosion.tbl'))
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b);base=0x30000000;u.mem_map(base,65536);stack=base+0xe000;stop=base+0xf000
u.mem_write(0x7bd99c,struct.pack('<I',len(names)))
for i,n in enumerate(names):u.mem_write(base+i*128,n+b'\0');u.mem_write(0x7b2870+i*8,struct.pack('<II',len(n),base+i*128))
queries=names+[n.upper() for n in names]+refs+[b''];missing=[]
for q in queries:
 u.mem_write(base+0x4000,q+b'\0');u.mem_write(stack,struct.pack('<II',stop,base+0x4000));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x497550,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 expected=next((i for i,n in enumerate(names) if n.lower()==q.lower()),0xffffffff);assert u.reg_read(UC_X86_REG_EAX)==expected,q
 if q in refs and expected==0xffffffff:missing.append(q.decode())
report=dict(result='PASS',cases=len(queries),missing_recipe_references=sorted(set(missing)),original_sha256=sha,scope='Original 497550, 5001d0 and CRT comparator execute unchanged against synthetic string objects populated from installed emitters.tbl. Actual loader/registration excluded.')
(root/'artifacts/emitter-lookup-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

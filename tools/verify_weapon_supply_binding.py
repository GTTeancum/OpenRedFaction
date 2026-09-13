"""Audit authored ammunition mappings and original SP active supply selection."""
import hashlib,json,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
inventory=json.loads((root/'artifacts/inventory.json').read_text())
def table(name):
 e=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
 with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
 return re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
ammo=re.findall(r'(?im)^\s*\$Name:\s*"([^"\r\n]+)"',table('ammo.tbl'));assert len(ammo)==16
weapons=table('weapons.tbl');records=[]
for match in re.finditer(r'(?im)^\s*\$Name:\s*"([^"\r\n]+)"([\s\S]*?)(?=^\s*\$Name:|^\s*#End)',weapons):
 name,block=match.groups();capacity=re.search(r'(?im)^\s*\$Max Ammo:\s*(-?\d+)\s+(-?\d+)',block);kind=re.search(r'(?im)^\s*\$Ammo Type:\s*"([^"\r\n]*)"',block);clip=re.search(r'(?im)^\s*\$Clip Size:\s*(-?\d+)\s+(-?\d+)',block);assert capacity and kind
 records.append(dict(name=name,ammo=kind[1],capacity=list(map(int,capacity.groups())),magazine=list(map(int,clip.groups())) if clip else [0,0]))
assert len(records)==44
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,0x10000);STACK=B+0xe000;STOP=B+0xf000
u.mem_write(0x85c760,w(len(ammo)))
for i,name in enumerate(ammo):
 text=name.encode();u.mem_write(B+i*256,text+b'\0');u.mem_write(0x85c4e0+i*20,w(len(text),B+i*256))
for i,r in enumerate(records):
 text=r['ammo'].encode();u.mem_write(B+0x3000,text+b'\0');u.mem_write(STACK,w(STOP,B+0x3000));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x4c22b0,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;index=u.reg_read(UC_X86_REG_EAX);expected=next((j for j,n in enumerate(ammo) if n.lower()==r['ammo'].lower()),0xffffffff);assert index==expected;r['ammo_index']=index if index!=0xffffffff else -1
 base=0x85cd08+i*0x550;u.mem_write(base+0x258,w(*r['capacity'],0xa5a5a5a5));u.mem_write(base+0x80,w(*r['magazine'],0xa5a5a5a5))
u.mem_write(0x872448,w(len(records)));u.mem_write(0x593e58,w(2));u.mem_write(STACK,w(STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x4c2a20,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP and u.mem_read(0x593e54,4)==w(2)
for i,r in enumerate(records):
 base=0x85cd08+i*0x550;assert u.mem_read(base+0x260,4)==w(r['capacity'][0]);assert u.mem_read(base+0x88,4)==w(r['magazine'][0]);r['sp_capacity']=r['capacity'][0];r['sp_magazine']=r['magazine'][0]
report=dict(result='PASS',ammo_types=ammo,weapons=records,scope='Actual4c22b0 lookup over authored ammo names and full4c2a20 SP descriptor selection. Numeric/name table extraction supplied independently; full original parser, compiled port reader and retained scene inventories are not covered.')
(root/'artifacts/weapon-supply-binding.json').write_text(json.dumps(report,indent=2));print(dict(result='PASS',ammo_types=len(ammo),weapons=len(records),no_ammo=[r['name'] for r in records if r['ammo_index']<0],scope=report['scope']))

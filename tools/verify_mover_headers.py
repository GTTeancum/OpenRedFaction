"""Compare mover UID/pose headers and trailer consumption with original readers."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,0x800000);data_at=base+4096;stack=base+0x700000
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());movers=json.loads((root/'artifacts/movers.json').read_text());count=0
for level in movers['results']:
 directory=next(l for l in levels if l['file']==level['file'] and l['archive']==level['archive']);section=next(s for s in directory['sections'] if s['type']=='0x2000')
 archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
 assert len(data)<0x600000;u.mem_write(data_at,data)
 for record in level['records']:
  u.mem_write(base,bytes(128));u.mem_write(base,struct.pack('<5I',1,0,data_at,record['offset'],len(data)));u.mem_write(base+0x50,struct.pack('<I',180));u.mem_write(stack,bytes(128));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
  u.emu_start(0x463c8e,0x463cbd,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x463cbd
  begin=record['offset'];raw=data[begin:begin+52]
  assert u.reg_read(UC_X86_REG_EBX)==struct.unpack_from('<I',raw)[0]
  assert bytes(u.mem_read(stack+0x10,48))==raw[4:16]+raw[28:52]+raw[16:28],(level['file'],record['uid'])
  assert struct.unpack('<I',u.mem_read(base+12,4))[0]==record['geometry_offset']
  u.mem_write(base+12,struct.pack('<I',record['geometry_offset']+record['geometry_bytes']));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base)
  u.reg_write(UC_X86_REG_ECX,base)
  u.emu_start(0x463cce,0x463ce5,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x463ce5
  assert struct.unpack('<I',u.mem_read(base+12,4))[0]==record['offset']+record['bytes']
  count+=1
report=dict(result='PASS',records=count,levels=len(movers['results']),scope='Original 463c8e..463cbd UID/position/orientation readers and 463cce..463ce5 v180 trailer reads unchanged. All 1406 inventory headers and trailer cursor boundaries checked. Embedded geometry parser/resource allocation and object creation are not executed by this check.')
(root/'artifacts/mover-header-verification.json').write_text(json.dumps(report,indent=2));print(report)

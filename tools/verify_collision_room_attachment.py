"""Run original room attachment on every loaded room's initial face views."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,4*1024*1024);stack=base+4000000;stop=stack+1024;face_base=base+4096
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/geometry.json').read_text())
rooms=faces=expanded=0
for level,tight in [(level,False) for level in levels]+[(levels[0],True)]:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--room-layout-tight' if tight else '--room-layout',str(Path(inventory['root'])/level['archive']),level['file']]);at=0
 for room in range(level['rooms']):
  initial=raw[at:at+24];want=raw[at+24:at+48];count=struct.unpack_from('<I',raw,at+48)[0];at+=52;records=[]
  for i in range(count):
   records.append((struct.unpack_from('<I',raw,at)[0],raw[at+4:at+28]));at+=28
  records.sort();assert len(set(i for i,b in records))==count
  assert count*128+4096<3900000
  u.mem_write(base,bytes(512));u.mem_write(base+8,initial)
  for i,(identity,bounds) in enumerate(records):
   pointer=face_base+i*128;u.mem_write(pointer,bytes(128));u.mem_write(pointer+16,bounds)
   u.mem_write(stack,struct.pack('<II',stop,pointer));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
   u.emu_start(0x4ccec0,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
   assert struct.unpack('<I',u.mem_read(pointer+0x44,4))[0]==base
  assert bytes(u.mem_read(base+8,24))==want,(level['file'],room,'bounds')
  first,total=struct.unpack('<II',u.mem_read(base+0x28,8));assert total==count
  for i in range(count):
   assert first==face_base+i*128,(level['file'],room,'order')
   first=struct.unpack('<I',u.mem_read(first+0x5c,4))[0]
  assert first==0
  rooms+=1;faces+=count;expanded+=initial!=want
 assert at==len(raw)
assert expanded>0
report=dict(result='PASS',levels=len(levels),rooms=rooms,faces=faces,synthetic_rooms=levels[0]['rooms'],expanded_rooms=expanded,
 scope='Unmodified original 4ccec0 with append helper 4ce200 and min/max helpers; exact room bounds, owner pointers and append order for initial file faces. PC owned-room bounds compared. Does not execute original finalizer rejection or post-load mutations.')
(root/'artifacts/collision-room-attachment-verification.json').write_text(json.dumps(report,indent=2));print(report)

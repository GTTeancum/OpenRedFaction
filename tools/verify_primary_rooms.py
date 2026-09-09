"""Initial constructor append and detail routing versus loaded primary indices."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,4*1024*1024);rooms=base+4096;arrays=base+2*1024*1024;stack=base+3*1024*1024;stop=stack+1024
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,4*1024*1024)
match=re.search(r'_rf_geometry_primary_rooms\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match;entry=int(match.group(1),16)
levels=json.loads((root/'artifacts/geometry.json').read_text());fixtures=[];primary_total=room_total=0
for level in levels:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(root/'Installed_Game'/level['archive']),level['file'],'--primary'])
 flags,expected=[list(map(int,line.split())) for line in raw.splitlines()];count=flags.pop(0);n=expected.pop(0)
 assert len(flags)==count==level['rooms'] and len(expected)==n
 fixtures.append((flags,expected));room_total+=count;primary_total+=n
rng=random.Random(0x4ce110)
for n in range(500):
 flags=[rng.choice([0,0,1,2,255]) for _ in range(n%33)]
 fixtures.append((flags,[i for i,f in enumerate(flags) if not f]))
for flags,expected in fixtures:
 count=len(flags);u.mem_write(base,bytes(512))
 for j,offset in enumerate((0x90,0x9c,0xa8)):u.mem_write(base+offset,struct.pack('<III',0,count,arrays+j*16384))
 assert count*512<2*1024*1024-4096 and count*4<16384
 for i,flag in enumerate(flags):
  room=rooms+i*512;u.mem_write(room,bytes(512))
  # Constructor tail writes all-room index and appends to both arrays.
  u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base+0x90);u.reg_write(UC_X86_REG_EDI,base);u.reg_write(UC_X86_REG_EBP,room);u.reg_write(UC_X86_REG_EAX,i)
  u.emu_start(0x4ccdbf,0x4ccdd6,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4ccdd6
  u.mem_write(stack,struct.pack('<III',stop,base,flag));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,room)
  u.emu_start(0x4ce110,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 for offset,want in [(0x90,list(range(count))),(0x9c,expected),(0xa8,[i for i,f in enumerate(flags) if f])]:
  n,cap,pointer=struct.unpack('<III',u.mem_read(base+offset,12));actual=[(v-rooms)//512 for v in struct.unpack('<'+str(n)+'I',u.mem_read(pointer,n*4))];assert actual==want
 x.mem_write(base,bytes(68));x.mem_write(base,struct.pack('<I',rooms));x.mem_write(base+16,struct.pack('<I',count));x.mem_write(base+52,struct.pack('<I',arrays))
 for i,flag in enumerate(flags):x.mem_write(arrays+i*4,struct.pack('<I',i*64));x.mem_write(rooms+i*64+34,bytes([flag]))
 output=arrays+16384;written=output+16384;n=len(expected)
 for cap,status in [(n,0)]+([(n-1,0xfffffffc)] if n else []):
  x.mem_write(output,bytes([0xa5])*(n*4+4));x.mem_write(written,bytes([0xa5])*4)
  x.mem_write(stack,struct.pack('<5I',stop,base,output,cap,written));x.reg_write(UC_X86_REG_ESP,stack)
  x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==status
  if status:assert bytes(x.mem_read(output,n*4+4))==bytes([0xa5])*(n*4+4) and bytes(x.mem_read(written,4))==bytes([0xa5])*4
  else:assert struct.unpack('<I',x.mem_read(written,4))[0]==n and list(struct.unpack('<'+str(n)+'I',x.mem_read(output,n*4)))==expected
report=dict(result='PASS',levels=len(levels),rooms=room_total,primary_rooms=primary_total,synthetic_cases=500,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Original constructor tail 4ccdbf..4ccdd6 and complete detail setter 4ce110, with unchanged stable removal and append helpers. Preallocated arrays. Loaded PC and NXDK primary ordering, synthetic byte values/empty lists and NXDK capacity guards. Initial state only, not later toggles.')
(root/'artifacts/primary-rooms-verification.json').write_text(json.dumps(report,indent=2));print(report)

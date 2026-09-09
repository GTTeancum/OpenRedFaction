"""Verify explicit child-room lists against the original binary loader block."""
import hashlib,json,re,struct,subprocess,sys,tempfile
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,16*1024*1024);rooms=base+4096;array=base+4*1024*1024;lists=base+5*1024*1024;stream=base+8*1024*1024;payload=base+9*1024*1024;stack=base+15*1024*1024
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,16*1024*1024)
match=re.search(r'_rf_geometry_room_children\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match;entry=int(match.group(1),16)
levels=json.loads((root/'artifacts/geometry.json').read_text());fixtures=[];room_total=link_total=0
for level in levels:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(root/'Installed_Game'/level['archive']),level['file'],'--links'])
 lines=[list(map(int,line.split())) for line in raw.splitlines()];count,records=lines[0];assert count==level['rooms']
 source=lines[1:1+records];want=lines[1+records:];assert len(want)==count
 fixtures.append((count,source,want));room_total+=count;link_total+=sum(row[0] for row in want)
fixtures.append((3,[[1,2,2,2],[0,1,1],[1,1,0],[2,0]],[[1,1],[3,2,2,0],[0]]))
for count,source,want in fixtures:
 wire=b''.join(struct.pack('<'+str(len(row))+'I',*row) for row in source)
 u.mem_write(base,bytes(512));u.mem_write(base+0x90,struct.pack('<III',count,count,array));u.mem_write(array,struct.pack('<'+str(count)+'I',*[rooms+i*512 for i in range(count)]))
 at=lists
 for i,row in enumerate(want):
  u.mem_write(rooms+i*512,bytes(512));u.mem_write(rooms+i*512+0x6c,struct.pack('<III',0,max(row[0],1),at));at+=max(row[0],1)*4
 u.mem_write(stream,bytes(96));u.mem_write(stream,struct.pack('<5I',1,0,payload,0,len(wire)));u.mem_write(stream+0x50,struct.pack('<I',180));u.mem_write(payload,wire)
 u.mem_write(stack+0x14,struct.pack('<I',len(source)));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,stream);u.reg_write(UC_X86_REG_EDI,base+0x90)
 if source:u.emu_start(0x4edc44,0x4edc8c,count=10000000);assert u.reg_read(UC_X86_REG_EIP)==0x4edc8c
 assert struct.unpack('<I',u.mem_read(stream+12,4))[0]==len(wire)
 x.mem_write(base,bytes(68));x.mem_write(base,struct.pack('<I',payload));x.mem_write(base+16,struct.pack('<I',count));x.mem_write(base+60,struct.pack('<II',0,len(source)));x.mem_write(payload,wire)
 for i,row in enumerate(want):
  n,capacity,pointer=struct.unpack('<III',u.mem_read(rooms+i*512+0x6c,12));actual=[(j-rooms)//512 for j in struct.unpack('<'+str(n)+'I',u.mem_read(pointer,n*4))];assert [n]+actual==row
  for cap,status in [(n,0)]+([(n-1,0xfffffffc)] if n else []):
   x.mem_write(lists,bytes([0xa5])*(n*4+4));x.mem_write(array,bytes([0xa5])*4)
   x.mem_write(stack,struct.pack('<6I',stack+1024,base,i,lists,cap,array));x.reg_write(UC_X86_REG_ESP,stack)
   x.emu_start(entry,stack+1024,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stack+1024 and x.reg_read(UC_X86_REG_EAX)==status
   if status:assert bytes(x.mem_read(lists,n*4+4))==bytes([0xa5])*(n*4+4) and bytes(x.mem_read(array,4))==bytes([0xa5])*4
   else:assert struct.unpack('<I',x.mem_read(array,4))[0]==n and list(struct.unpack('<'+str(n)+'I',x.mem_read(lists,n*4)))==actual
# Minimal valid v180 level in a temporary single-entry VPP, then malformed links.
geometry=bytes(6)+struct.pack('<III',0,0,2)+bytes(84)+struct.pack('<4I',1,0,1,1)+bytes(20)
def packed_archive(payload):
 rfl=bytearray(32);struct.pack_into('<6I',rfl,0,0xd4bada55,180,0,32,88,3)
 rfl+=struct.pack('<II',0x70000,48)+bytes(48)+struct.pack('<III',0x1000000,4,0)
 rfl+=struct.pack('<II',0x100,len(payload))+payload+bytes(8)
 archive=bytearray(4096+((len(rfl)+2047)//2048)*2048)
 struct.pack_into('<4I',archive,0,0x51890ace,1,1,len(archive));archive[2048:2057]=b'test.rfl\0';struct.pack_into('<I',archive,2108,len(rfl));archive[4096:4096+len(rfl)]=rfl
 return archive
with tempfile.TemporaryDirectory(prefix='rf-room-links-') as temporary:
 path=Path(temporary)/'fixture.vpp'
 for offset,value in [(None,None),(106,2),(114,2),(110,0xffffffff)]:
  payload_copy=bytearray(geometry)
  if offset is not None:struct.pack_into('<I',payload_copy,offset,value)
  path.write_bytes(packed_archive(payload_copy))
  run=subprocess.run([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(path),'test.rfl','--links'],capture_output=True)
  assert (run.returncode==0 if offset is None else run.returncode==1 and b'error -2' in run.stderr),(offset,run.returncode,run.stderr)
report=dict(result='PASS',levels=len(levels),rooms=room_total,links=link_total,synthetic_cases=1,malformed_parser_guards=3,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Original 4edc44..4edc8c with unchanged binary reader, index lookup and append helpers; preallocated arrays avoid growth. Ordered PC and NXDK child indices and insufficient-capacity preservation. Synthetic repeated parent, duplicate child and empty list case included.')
(root/'artifacts/room-links-verification.json').write_text(json.dumps(report,indent=2));print(report)

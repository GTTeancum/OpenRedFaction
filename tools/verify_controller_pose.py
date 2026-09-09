"""Controller factory pose fields vs original copy/flag/empty-sphere helpers."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=root/'Installed_Game/RF.exe';assert hashlib.sha256(p.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(p)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);params=base+4096;stack=base+50000;stop=base+64000
fields=[(0x7c,4),(0x180,4),(0x238,12),(0x244,36),(0xe4,12),(0x3c,12),(0xf0,12),(0x144,12),(0x48,36),(0xfc,36),(0x120,36),(0x190,12),(0x19c,12)]
records=[g['keys'][0] for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] for g in l['records'] if g['keys']];cases=[];expected=[]
for n,r in enumerate(records):
 disk=r['orientation_disk'];wire=struct.pack('<12f',*r['position'],*(disk[3:]+disk[:3]));cases.append(wire)
 u.mem_write(base,bytes([0xa5])*1024);u.mem_write(params,bytes(152));u.mem_write(params+0x3c,wire[:12]);u.mem_write(params+0x48,wire[12:]);u.mem_write(base+0x184,bytes(12))
 # Factory matrix/base copies and flags with original type-8 parameter flags=1.
 u.mem_write(stack+0x3c,struct.pack('<I',1));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EDI,params+0x3c);u.reg_write(UC_X86_REG_EBP,params);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x486ee6,0x486f63,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x486f63
 # Physics pose-copy block, with zero initial velocity parameters.
 u.reg_write(UC_X86_REG_ESI,base+0x88);u.reg_write(UC_X86_REG_EDI,params);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x49f051,0x49f0ab,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49f0ab
 # Complete radius/bounds rebuild with an empty sphere list: overwritten poison.
 u.mem_write(stack,struct.pack('<2I',stop,base+0x88));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4a0cb0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop and bytes(u.mem_read(base+0x180,4))==bytes(4)
 # Public factory position copy through unchanged vector assignment helper.
 u.mem_write(stack,struct.pack('<3I',stop,stack+512,params+0x3c));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base+0x3c)
 u.emu_start(0x409f40,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 blob=bytes(u.mem_read(base,1024));expected.append(struct.pack('<i',0)+b''.join(blob[a:a+size] for a,size in fields))
for offset in (0,12):
 wire=bytearray(cases[0]);struct.pack_into('<I',wire,offset,0x7fc00000);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*236)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--controller-pose'],input=b''.join(cases));assert len(pc)==240*len(cases)
for n,want in enumerate(expected):assert pc[n*240:(n+1)*240]==want,(n,'PC')
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(b,(len(im)+4095)//4096*4096);x.mem_write(b,im);x.mem_map(base,65536)
entry=int(re.search(r'_rf_group_controller_pose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 x.mem_write(base,bytes([0xa5])*236);x.mem_write(params,bytes(356));x.mem_write(params+24,wire);x.mem_write(stack,struct.pack('<3I',stop,params,base));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,236));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',controller_keys=len(records),guards=2,scope='Original factory base/matrix/flag and physics pose blocks plus complete empty-sphere 4a0cb0 and public-position vector copy. PC/NXDK mapped pose bytes match all first keys. No complete allocator/factory, registration or runtime scene binding.')
(root/'artifacts/controller-pose-verification.json').write_text(json.dumps(report,indent=2));print(report)

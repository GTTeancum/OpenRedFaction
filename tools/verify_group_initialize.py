"""Translation constructor state blocks against PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=root/'Installed_Game/RF.exe';assert hashlib.sha256(p.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(p)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);key=base+4096;array=base+8192;group=base+16384;stack=base+50000
fields=[(0x7c,4),(0x180,4),(0x238,12),(0x244,36),(0xe4,12),(0x3c,12),(0xf0,12),(0x144,12),(0x48,36),(0xfc,36),(0x120,36),(0x190,12),(0x19c,12)]
def mapped(blob):return b''.join(blob[a:a+n] for a,n in fields)
def runtime(blob):return b''.join(blob[a:a+n] for a,n in [(0x318,4),(0x2e8,4),(0x2f8,8),(0x300,4),(0x30c,4),(0x2f4,4),(0x304,4),(0x310,4),(0x7c,4),(0xe4,12),(0xf0,12),(0x144,12)])
records=[(g,i,k) for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] for g in l['records'] for i,k in enumerate(g['keys'])]
rng=random.Random(0x4695f5);cases=[];expected=[]
for n,(g,index,k) in enumerate(records):
 flags=rng.getrandbits(32)&~4;mode=n%6;now=[0,1000,1072800000][n%3];position=struct.pack('<3f',*k['position']);count=len(g['keys'])
 blob=bytearray([0xa5])*1024;struct.pack_into('<I',blob,0x7c,rng.getrandbits(32));struct.pack_into('<f',blob,0x180,[-1,0,.25,8][n%4]);struct.pack_into('<I',blob,0x318,flags);struct.pack_into('<I',blob,0x2e8,mode);struct.pack_into('<i',blob,0x30c,-1);struct.pack_into('<3I',blob,0x29c,count,count,array)
 cases.append(mapped(blob)+struct.pack('<4Ii',flags,mode,index,count,now)+position)
 u.mem_write(base,bytes(blob));u.mem_write(key,bytes(128));u.mem_write(key+4,position);u.mem_write(array,struct.pack('<'+'I'*count,*([key]*count)));u.mem_write(group,bytes(128));u.mem_write(group+0x34,struct.pack('<I',index));u.mem_write(0x5a3ed8,struct.pack('<i',now));u.mem_write(0x64ecb9,bytes(2))
 u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBP,0);u.reg_write(UC_X86_REG_EDI,0xffffffff);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base+0x310);u.mem_write(stack,bytes(4));u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x469570,0x469593,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x469593
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base+0x29c);u.mem_write(stack+0xcc,struct.pack('<I',group))
 u.emu_start(0x4695f5,0x469662,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x469662
 actual=bytes(u.mem_read(base,1024));expected.append(struct.pack('<i',0)+runtime(actual)+mapped(actual))
original_count=len(cases)
for offset,value,status in [(236,4,-4),(240,6,-4),(244,0xffffffff,-4),(248,0,-4),(252,0xffffffff,-4),(256,0x7fc00000,-2)]:
 wire=bytearray(cases[0]);struct.pack_into('<I',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',status)+bytes([0xa5])*76+wire[:236])
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-initialize'],input=b''.join(cases));assert len(pc)==316*len(cases)
for n,want in enumerate(expected):assert pc[n*316:(n+1)*316]==want,(n,'PC')
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(b,(len(im)+4095)//4096*4096);x.mem_write(b,im);x.mem_map(base,65536);stop=base+64000
entry=int(re.search(r'_rf_group_translation_initialize\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 x.mem_write(base,wire);x.mem_write(base+1024,bytes([0xa5])*76);x.mem_write(key,bytes(356));x.mem_write(key+24,wire[256:268]);flags,mode,index,count,now=struct.unpack_from('<5I',wire,236)
 x.mem_write(stack,struct.pack('<9I',stop,base+1024,base,flags,mode,key,index,count,now));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+1024,76))+bytes(x.mem_read(base,236));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',original_cases=original_count,guards=6,scope='Original 469570..469593 reset/timer block and 4695f5..469662 selected translation-key pose block with unchanged helpers, all authored key positions and varied selected indices/radii/modes/flags/clocks. Full mapped pose/runtime bytes match PC/NXDK. Not complete constructor, factory/base initialization, rotation, sound or registration.')
(root/'artifacts/group-initialize-verification.json').write_text(json.dumps(report,indent=2));print(report)

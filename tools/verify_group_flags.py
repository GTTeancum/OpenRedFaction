"""Initial group flags vs original registration blocks and NXDK."""
import hashlib,itertools,json,math,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=root/'Installed_Game/RF.exe';assert hashlib.sha256(p.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(p)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);obj=base;group=base+4096;key=base+8192;array=base+10000;stack=base+60000;stop=base+64000
records=[g for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] for g in l['records']]
cases=[(len(g['keys']),g['flags'],g['keys'][0]['timing'][3:5]) for g in records if g['keys']]
installed=len(cases)
for flags in itertools.product([0,1,2,255],repeat=6):cases.append((1,flags,[(0,-0.,.5,-1)[sum(flags)%4],(0,1)[sum(flags)%2]]))
expected=[];wires=[];gated=0
for count,flags,timing in cases:
 wire=struct.pack('<I6B2x2f',count,*flags,*timing);wires.append(wire)
 u.mem_write(obj,bytes(1024));u.mem_write(group,bytes(256));u.mem_write(group+0x28,bytes(int(bool(f)) for f in flags));u.mem_write(group+12,struct.pack('<I',array));u.mem_write(array,struct.pack('<I',key));u.mem_write(key+0x40,wire[12:20]);u.mem_write(stack,bytes(256));u.mem_write(stack+0xcc,struct.pack('<I',group))
 u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_EBX,group);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4693c9,0x469404,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x469404
 u.emu_start(0x46949a,0x469570,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x469570
 value,=struct.unpack('<I',u.mem_read(obj+0x318,4));expected.append(struct.pack('<iI',0,value))
 u.mem_write(stack,struct.pack('<II',stop,obj));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x46b320,stop,count=100);assert u.reg_read(UC_X86_REG_EIP)==stop;gate=u.reg_read(UC_X86_REG_EAX)&255;assert gate==bool(value&4);gated+=gate
for count,timing,status in [(0,[0,0],-3),(1,[math.nan,0],-2),(1,[0,math.inf],-2)]:wires.append(struct.pack('<I6B2x2f',count,*([0]*6),*timing));expected.append(struct.pack('<iI',status,0xa5a5a5a5))
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-flags'],input=b''.join(wires));assert raw==b''.join(expected)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();xb=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(im)+4095)//4096*4096);x.mem_write(xb,im);x.mem_map(base,65536);entry=int(re.search(r'_rf_level_group_initial_flags\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(wire,want) in enumerate(zip(wires,expected)):
 x.mem_write(group,bytes(1352));x.mem_write(key,bytes(356));x.mem_write(group+1292,wire[:4]);x.mem_write(group+1330,wire[4:10]);x.mem_write(key+84,wire[12:20]);x.mem_write(obj,bytes([0xa5])*4);x.mem_write(stack,struct.pack('<4I',stop,group,key,obj));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(obj,4));assert got==want,(i,got.hex(),want.hex())
report=dict(result='PASS',installed_groups=installed,synthetic_cases=4096,port_guards=3,gate_true=gated,scope='Original initial flag blocks 4693c9..469404 and 46949a..469570 with real first-key accessor, normalized file booleans; complete 46b320 gate. Exact PC/NXDK output, errors preserve flags. Not complete registration, sound setup, runtime attachment or playback.')
(root/'artifacts/group-flags-verification.json').write_text(json.dumps(report,indent=2));print(report)

"""C controller position commit against complete original 46a8f0."""
import json,re,runpy,struct,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/probe_group_pose_commit.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
cases=o['port_cases'];expected=[struct.pack('<i',0)+v for v in o['port_expected']]
# Late invalid target: no controller/earlier target mutation or dirty clearing.
wire=bytearray(cases[1]);struct.pack_into('<3I',wire,0,8,2,0);struct.pack_into('<2I',wire,12,0x12340001,0x12340008);struct.pack_into('<I',wire,140+8*236+80,0x7fc00000)
cases.append(bytes(wire));expected.append(struct.pack('<iI',-2,8)+wire[140:])
# Clean controllers do not inspect pending values at all.
struct.pack_into('<I',wire,0,0);cases.append(bytes(wire));expected.append(struct.pack('<iI',0,0)+wire[140:])
# A controller may occur in its own attached list.
wire=bytearray(cases[1]);struct.pack_into('<3I',wire,0,8,1,0);struct.pack_into('<I',wire,12,0x12340000)
# Use the already original-checked position assignment semantics for this alias guard.
pose=bytearray(wire[140:376]);q=pose[80:92];radius,=struct.unpack_from('<f',pose,4);v=struct.unpack('<3f',q)
for offset in (56,68,80):pose[offset:offset+12]=q
pose[212:224]=struct.pack('<3f',*[p-radius for p in v]) if radius>0 else q
pose[224:236]=struct.pack('<3f',*[p+radius for p in v]) if radius>0 else q
flags,=struct.unpack_from('<I',pose);struct.pack_into('<I',pose,0,flags|0x4000000)
cases.append(bytes(wire));expected.append(struct.pack('<iI',0,0)+pose+wire[376:])
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-commit'],input=b''.join(cases));stride=8+236*9;assert len(pc)==stride*len(cases)
for n,want in enumerate(expected):assert pc[n*stride:(n+1)*stride]==want,(n,'PC')
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(b,(len(im)+4095)//4096*4096);x.mem_write(b,im);base=0x31000000;x.mem_map(base,65536);stack=base+50000;stop=base+64000
entry=int(re.search(r'_rf_group_commit_positions\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 x.mem_write(base,wire);flags,movers,general=struct.unpack_from('<3I',wire)
 x.mem_write(base+4096,struct.pack('<6I',0,0,base+12,movers,base+76,general))
 x.mem_write(base+8192,b''.join(struct.pack('<2I',0x12340000+i,base+140+i*236) for i in range(9)))
 x.mem_write(stack,struct.pack('<6I',stop,base,base+140,base+4096,base+8192,9));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,4))+bytes(x.mem_read(base+140,236*9));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',original_controllers=len(o['port_expected']),original_objects=len(o['port_expected'])*9,additional_cases=3,scope='PC/NXDK vs complete original 46a8f0: dirty gating/clearing, self and attached positions, both lists, duplicates/stale/missing handles, positive/nonpositive radius. Complete mapped pose bytes checked. Additional late failure preservation, clean invalid data and self-list alias checks. No scene/runtime mirror integration yet.')
(root/'artifacts/group-commit-verification.json').write_text(json.dumps(report,indent=2));print(report)

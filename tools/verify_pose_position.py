"""C object position assignment against complete original 48a230 calls."""
import json,re,runpy,struct,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/verify_mover_pose_initialization.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
cases=o['port_cases'];expected=[struct.pack('<i',0)+v for v in o['port_expected']]
# Finite-contract failures must preserve the complete pose.
for offset,bits in [(236,0x7fc00000),(4,0x7f800000),(236,0x7f7fffff)]:
 wire=bytearray(cases[1]);struct.pack_into('<I',wire,248,0);struct.pack_into('<I',wire,offset,bits)
 if bits==0x7f7fffff:struct.pack_into('<I',wire,4,bits)
 cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+wire[:236])
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--pose-position'],input=b''.join(cases));assert len(pc)==240*len(cases)
for n,want in enumerate(expected):assert pc[240*n:240*n+240]==want,(n,'PC',pc[240*n:240*n+4],want[:4])
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(b,(len(im)+4095)//4096*4096);x.mem_write(b,im);base=0x31000000;x.mem_map(base,65536);stack=base+50000;stop=base+64000
entry=int(re.search(r'_rf_group_pose_set_position\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 x.mem_write(base,wire);alias,=struct.unpack_from('<I',wire,248)
 x.mem_write(stack,struct.pack('<3I',stop,base,base+(80 if alias else 236)));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,236));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',original_cases=len(o['port_expected']),guards=3,scope='PC/NXDK position assignment vs complete original 48a230; full mapped pose bytes, authored/random positions, positive/nonpositive radii, aliased pending source. Finite-contract guards preserve pose. Controller commit lists and scene integration remain open.')
(root/'artifacts/pose-position-verification.json').write_text(json.dumps(report,indent=2));print(report)

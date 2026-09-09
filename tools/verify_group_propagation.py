"""C attached translation propagation vs full original function fixtures."""
import json,re,runpy,struct,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/probe_group_propagation.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
cases=list(o['port_cases']);expected=[struct.pack('<i',0)+p for p in o['port_expected']]
assert all(len(c)==360 for c in cases) and all(len(e)==240 for e in expected)
seed=next(c for c in cases if struct.unpack_from('<I',c)[0]>0)
for kind in range(4):
 wire=bytearray(seed);struct.pack_into('<I',wire,4,1)
 if kind==0:struct.pack_into('<I',wire,0,5);status=-4
 elif kind==1:struct.pack_into('<I',wire,248+24,4);status=-4
 elif kind==2:struct.pack_into('<I',wire,248,0x7fc00000);status=-2
 else:struct.pack_into('<If',wire,4,0,0.);struct.pack_into('<I',wire,248+24,8);status=-2
 cases.append(bytes(wire));expected.append(struct.pack('<i',status)+wire[12:248])
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-propagate'],input=b''.join(cases));assert len(pc)==240*len(cases)
for n,want in enumerate(expected):assert pc[n*240:n*240+240]==want,(n,'PC')
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(b,(len(im)+4095)//4096*4096);x.mem_write(b,im);base=0x31000000;x.mem_map(base,65536);stack=base+50000;stop=base+64000
entry=int(re.search(r'_rf_group_translation_propagate\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 count,force,dtbits=struct.unpack_from('<3I',wire);x.mem_write(base,wire);x.mem_write(stack,struct.pack('<6I',stop,base+12,base+248,count,dtbits,force));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop,(n,hex(x.reg_read(UC_X86_REG_EIP)))
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+12,236));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',original_cases=2000,port_guards=4,scope='PC/NXDK C propagation compared against mapped pose bytes from complete original 46bbe0. Caller-supplied accepted ordered contributions; dirty gating, forced/normal updates, all three matrices, positions, velocity and bounds. Count/unsupported rotation/nonfinite/zero-dt guards preserve state. No C registry collection, flag-800 alignment, rotation or scene integration.')
(root/'artifacts/group-propagation-verification.json').write_text(json.dumps(report,indent=2));print(report)



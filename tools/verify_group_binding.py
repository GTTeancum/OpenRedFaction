"""Controller handle lists through C binding/propagation vs full original function."""
import json,re,runpy,struct,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/probe_group_propagation.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
cases=o['binding_cases'];expected=[struct.pack('<i',0)+v for v in o['port_expected']];assert all(len(c)==320 for c in cases)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-bind'],input=b''.join(cases));assert len(pc)==240*len(cases)
for n,want in enumerate(expected):assert pc[n*240:n*240+240]==want,(n,'PC')
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(b,(len(im)+4095)//4096*4096);x.mem_write(b,im);base=0x31000000;x.mem_map(base,65536);stack=base+50000;stop=base+64000
entry=int(re.search(r'_rf_group_translation_bind_pose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 count,force,dtbits=struct.unpack_from('<3I',wire);x.mem_write(base,wire)
 for i in range(2):
  start=248+36*i;runtime=bytearray(76);runtime[:4]=wire[start+24:start+28];runtime[52:64]=wire[start+12:start+24];key=bytearray(356);key[24:36]=wire[start:start+12]
  x.mem_write(base+1024+i*76,bytes(runtime));x.mem_write(base+2048+i*356,bytes(key));x.mem_write(base+4096+i*24,struct.pack('<6I',base+1024+i*76,base+2048+i*356,base+start+28,1,base+start+32,1))
 x.mem_write(stack,struct.pack('<7I',stop,base+12,0x12340000,base+4096,count,dtbits,force));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+12,236));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',original_cases=2000,scope='Raw controller mover/general handle lists through PC/NXDK binding and propagation vs complete original 46bbe0 pose bytes. Ordered duplicate references, stale/missing handles, general-list exclusion, dirty gating and both force modes. Target is a known registered handle; full registry allocation, authored controller ownership, rotation and scene integration remain open.')
(root/'artifacts/group-binding-verification.json').write_text(json.dumps(report,indent=2));print(report)

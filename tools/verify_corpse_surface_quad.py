"""Original surface quad construction against actual PC/NXDK instructions."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(data)+4095)//4096*4096);x.mem_write(base,data)
b=0x30000000;x.mem_map(b,65536);quad=b+0x1000;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'\s_rf_corpse_surface_build_quad\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inputs=[];outputs=[]
def observe(raw,after,vertices):
 x.mem_write(b,raw);x.mem_write(quad,bytes(84));x.mem_write(stack,struct.pack('<3I',stop,b,quad))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 actual=bytes(x.mem_read(b,84))+bytes(x.mem_read(quad,84));expected=after+vertices
 assert actual==expected,('NXDK',len(inputs),[(i,a,z) for i,(a,z) in enumerate(zip(actual,expected)) if a!=z][:20])
 inputs.append(raw);outputs.append(expected)
runpy.run_path(str(root/'tools/verify_corpse_surface_draw_original.py'),init_globals={'observe_case':observe})
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-surface-quad'],input=b''.join(inputs))
assert actual==b''.join(outputs),'PC output differs'
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Exact full effect payload and84-byte vertex/UV/color output vs original42df20 in512 synthetic cases. Both actual PC and compiled NXDK code, including native libm, executed. Original final renderer submission is supplied. No claim of universal sine bit-equivalence or GPU/live dispatch integration.')
(root/'artifacts/corpse-surface-quad.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

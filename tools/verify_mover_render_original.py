"""Audit full488b20/type9 and46b2b0; only renderer/white setup supplied."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096)
u.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
u.mem_map(B,0x10000)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
def hook(cpu,address,size,context):
 if address not in (0x50cf80,0x516df0):return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x516df0:
  assert [r(sp+4+i*4) for i in range(4)]==[B+0x1000,B+0x3c,B+0x48,0]
 trace.append(address)
 cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)
results=[]
for flags,disabled,solid in itertools.product((0,2,16,0x4000,0x4002,0x6000000),(0,1,255),(0,B+0x1000)):
 u.mem_write(B,bytes(0x2000));u.mem_write(B+0x24,w(9));u.mem_write(B+0x7c,w(flags,0))
 u.mem_write(B+0x294,w(solid));u.mem_write(0x64e6dc,bytes([disabled]))
 u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK);trace.clear()
 u.emu_start(0x488b20,STOP,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP
 hidden=bool(flags&2);draw=not hidden and not disabled and bool(solid)
 # No model wrapper: flag4000 does not suppress the outer dispatch.
 expected=[] if hidden else [0x50cf80]+([0x516df0] if draw else [])
 assert trace==expected,(flags,disabled,solid,trace)
 expected_flags=flags if hidden else flags|16
 assert r(B+0x7c)==expected_flags
 results.append({'flags':flags,'disabled_byte':disabled,'solid':bool(solid),'draw':draw,'final_flags':expected_flags})
report={'result':'PASS','cases':len(results),'draws':sum(c['draw'] for c in results),
 'original_sha256':sha,'scope':'Original488b20 type9 and complete46b2b0 execute; white setup and516df0 graphics supplied. Solid/pose pointer order and outer marker on skipped family draw verified. No port renderer parity claim.','records':results}
out=root/'artifacts/analysis/mover-render-original.json';out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='records'}))

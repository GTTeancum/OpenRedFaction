"""Compare the shared support dispatch with original post-update execution."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
subprocess.run([sys.executable,str(root/'tools/inspect_jump_support.py')],cwd=root,check=True)
cases=json.loads((root/'artifacts/jump-support-dispatch.json').read_text())['results']
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
commands=[w(c['mode'],c['jump']*2,c['kind_one'],c['attachment'],c['parent'],c['moved'],c['supported']*0x400000,c['object_bit']*8) for c in cases]
expected=[w({'none':0,'fall':1,'support':2}[c['route']]) for c in cases]
probe=root/'build/pc/Release/rf_entity_probe.exe';assert subprocess.check_output([str(probe),'--support-route'],input=b''.join(commands))==b''.join(expected)
binary=root/'build/xbox/main.exe';pe=pefile.PE(str(binary));im=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase;pe.close()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(im)+4095)//4096*4096);u.mem_write(origin,im)
base=0x30000000;u.mem_map(base,65536);stack=base+0xe000;stop=base+0xf000
entry=int(re.search(r'_rf_player_support_route\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for command,expected_value in zip(commands,expected):
 u.mem_write(base,command);u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=1000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 assert w(u.reg_read(UC_X86_REG_EAX))==expected_value and bytes(u.mem_read(base,32))==command
u.mem_write(stack,w(stop,0));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=1000);assert u.reg_read(UC_X86_REG_EAX)==0
report=dict(result='PASS',cases=len(cases),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Resolved post-update support dispatch only. Does not execute support collision, the actor update or full player lifecycle.')
(root/'artifacts/support-route-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(cases),'shared PC/NXDK support routes')

"""Shared glare constructor versus original footprint and supplied services."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
original=json.loads((root/'artifacts/glare-create-original.json').read_text());assert original['result']=='PASS'
cases=[(a['index'],a['flag'],a['fail']) for a in original['records']]+[(i,flag,mode) for i in range(3) for flag in (0,257) for mode in (3,4)]
raw=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--glare-create'],input=b''.join(w(*a) for a in cases))
assert len(raw)==124*len(cases)
for i,a in enumerate(original['records']):
 out=raw[i*124:(i+1)*124];assert out[:20]==w(0,a['created'],a['trace'],1+a['created'],1+a['created']);assert out[20:]==bytes.fromhex(a['state'])
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x10000);C=B;L=B+0x100;PREV=B+0x200;STATE=B+0x300;BACK=B+0x500;OUT=B+0x600;S=B+0xf000;STOP=B+0xff00
callbacks=[B+0x8000,B+0x8100,B+0x8200];entry=int(re.search(r'\s_rf_glare_create\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
for i in range(3):x.mem_write(C+i*12,struct.pack('<2fI',*((.5,1),(1,.5),(1,1))[i],0x5c9e98+52*i))
x.mem_write(BACK,w(callbacks[1],callbacks[2],0));pose=struct.pack('<12f',1,0,0,0,1,0,0,0,1,1.25,-2.5,3.75)
mode=trace=0
def hook(cpu,address,size,context):
 global trace
 sp=rsp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+i*4);operation=callbacks.index(address)+1;trace=trace*10+operation;status=0
 if mode==operation+1:status=0xffffffff
 elif operation==1:
  assert arg(1)==0x3f000000 and arg(2)==0x3f800000;x.mem_write(arg(3),struct.pack('<f',.75))
 elif operation==2:
  assert arg(1)==123 and arg(2)==7;x.mem_write(arg(3),pose)
 else:
  d=arg(1);assert bytes(x.mem_read(d,56))==w(123,0x3f800000)+pose[36:]+pose[:36]
  x.mem_write(arg(2),w(0 if mode==1 else STATE))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,r(sp))
for address in callbacks:x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
for i,(index,flag,mode) in enumerate(cases):
 trace=0;x.mem_write(L,w(PREV,PREV,1,1));x.mem_write(PREV,w(L,L));x.mem_write(STATE,b'\xa5'*104);x.mem_write(STATE+52,bytes(8));x.mem_write(OUT,w(1))
 x.mem_write(S,w(STOP,C,3,index,123,7,flag,L,BACK,OUT));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 value=r(OUT);created=value==STATE;state=bytearray(x.mem_read(STATE,104))
 if created:
  assert r(PREV)==STATE+52 and r(L+4)==STATE+52 and r(STATE+52)==L and r(STATE+56)==PREV;state[52:60]=w(1,2)
 else:assert r(PREV)==L and r(L+4)==PREV
 actual=w(x.reg_read(UC_X86_REG_EAX),1 if created else 2 if value else 0,trace,r(L+8),r(L+12))+state
 assert actual==raw[i*124:(i+1)*124],(i,actual.hex(),raw[i*124:(i+1)*124].hex())
report=dict(result='PASS',original_cases=60,callback_failure_cases=12,scope='Shared PC and actual compiled NXDK constructor match full original413d20 normalized state and ordered service evidence, class bounds, null allocation, low-byte flags and list append. Additional tag/allocation callback failures preserve output/list on PC/NXDK. Only callbacks supplied; no live type10 allocation/glare rendering or native XEMU claim.')
(root/'artifacts/glare-create-shared.json').write_text(json.dumps(report,indent=2));print(report)

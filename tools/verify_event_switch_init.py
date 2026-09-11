"""Original Switch on-action vs PC/NXDK with linked/audio effects observed."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_event_switch_init\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
frame=base+0x8000;sound=base+0x5000;u.mem_write(sound,b'switch.wav\0')
calls=[]
def hook(m,address,size,context):
 if address not in (0x4ff480,0x4b6870,0x4ffa80,0x5054b0,0x5054d0):return
 sp=m.reg_read(UC_X86_REG_ESP);pop=0
 if address==0x4ff480:
  assert m.reg_read(UC_X86_REG_ECX)==frame+0x3c;m.reg_write(UC_X86_REG_EAX,sound)
 elif address==0x4b6870:
  assert bytes(m.mem_read(sp+4,8))==w(frame+0x5c,32);m.reg_write(UC_X86_REG_EAX,base)
 elif address==0x4ffa80:
  assert m.reg_read(UC_X86_REG_ECX)==base+0x2c0 and read(m,sp+4)==sound;pop=4
 elif address==0x5054b0:
  assert bytes(m.mem_read(sp+4,16))==w(sound,0x40a00000,0x3f800000,0x3f800000)
  if bytes(m.mem_read(sound,1))==b'\0':
   calls.append('empty-load');return # Execute actual5054b0 ->543580 empty-name path.
  m.reg_write(UC_X86_REG_EAX,audio_handle);calls.append('load')
 else:
  assert read(m,sp+4)==audio_handle;calls.append('preload')
 m.reg_write(UC_X86_REG_EIP,read(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,hook)
authored=[(l['file'],r) for l in json.loads((root/'artifacts/events.json').read_text())['results'] for r in l['records'] if r['type_index']==32]
cases=[(r['words'][0],r['words'][1],r['values'][0],r['flags'][0]) for _,r in authored]
import itertools
cases+=list(itertools.product((0,1,0xffffffff),(0,1,0xffffffff),(-2147483648.,-2.9,-.5,0.,.9,1.9,2.9,2147483520.),(0,1,255,256,257)))
commands=bytearray();expected=bytearray()
for index,(disabled,limit,mode,unlimited) in enumerate(cases):
 data=w(disabled,limit)+f(mode)+w(unlimited);raw=bytes([0xa5])*0x2d8;u.mem_write(base,raw)
 u.mem_write(frame,bytes(256));u.mem_write(frame+0x10,w(unlimited)+f(mode))
 u.reg_write(UC_X86_REG_ESI,disabled);u.reg_write(UC_X86_REG_EBX,limit);u.reg_write(UC_X86_REG_ESP,frame)
 audio_handle=0xffffffff if index%2 else 123;calls=[]
 u.emu_start(0x462626,0x46264b,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x46264b
 assert calls==(['load'] if audio_handle==0xffffffff else ['load','preload'])
 result=bytes(u.mem_read(base+0x2b8,8))+w(u.mem_read(base+0x2c8,1)[0])+bytes(u.mem_read(base+0x2cc,4))+bytes(u.mem_read(base+0x2d4,4))
 after=bytes(u.mem_read(base,len(raw)));assert after[:0x2b8]==raw[:0x2b8] and after[0x2c0:0x2c8]==raw[0x2c0:0x2c8] and after[0x2c9:0x2cc]==raw[0x2c9:0x2cc]
 assert read(u,base+0x2d0)==audio_handle
 commands.extend(data);expected.extend(w(0)+result)
 x.mem_write(base,b'\xa5'*20);x.mem_write(stack,w(stop,base,disabled,limit)+f(mode)+w(unlimited));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(base,20))==result,index
assert all(not row['texts'][0] for _,row in authored)
u.mem_write(sound,b'\0');u.mem_write(base,b'\xa5'*0x2d8)
u.mem_write(frame,bytes(256));u.mem_write(frame+0x10,w(1)+f(0))
u.reg_write(UC_X86_REG_ESI,0);u.reg_write(UC_X86_REG_EBX,1);u.reg_write(UC_X86_REG_ESP,frame);calls=[]
u.emu_start(0x462626,0x46264b,count=10000)
assert u.reg_read(UC_X86_REG_EIP)==0x46264b and read(u,base+0x2d0)==0xffffffff
assert calls==['empty-load'],calls
for mode in (float('nan'),float('inf'),-float('inf'),2147483648.,-2147483904.):
 commands.extend(w(1,1)+f(mode)+w(1));expected.extend(w(0xfffffffc)+b'\xa5'*20)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--switch-init'],input=commands)
assert actual==expected,[(i,actual[i:i+24].hex(),expected[i:i+24].hex()) for i in range(0,len(expected),24) if actual[i:i+24]!=expected[i:i+24]][:3]
report=dict(result='PASS',authored=len(authored),cases=len(cases),original_empty_sound=True,original_sha256=digest,scope='Prepared loader block462626 through setter4b83e0 with actual573528 float conversion. Factory, string access/copy and nonempty audio ownership supplied at boundaries; sound arguments and optional preload verified. All83 authored names are empty; an additional actual5054b0 ->543580 run returns -1 and skips preload. All authored Switch field sets plus truncation/low-byte edge cases exact PC/NXDK. Five invalid mode PC cases preserve output. No full RFL loader, nonempty sound ownership or linked initialization.')
(root/'artifacts/event-switch-init.json').write_text(json.dumps(report,indent=2));print(report)


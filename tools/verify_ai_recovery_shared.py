"""Shared408f20 recovery versus full original oracle on PC and NXDK."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
subprocess.run([sys.executable,str(root/'tools/verify_ai_recovery_original.py')],check=True)
records=json.loads((root/'artifacts/ai-recovery-original.json').read_text())['records']
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
commands=[];expected=[];codes={'actor':1,'linked':2,'stop':3,'start':4,'duration':5}
for rec in records:
 cfg=rec['input'];commands.append(w(*rec['initial'],cfg['now'],cfg['present'],cfg['linked'],cfg['health_bits'],cfg['duration_bits']))
 trace=[]
 for event in rec['trace']:
  if event[0] in codes:trace.extend((codes[event[0]],*event[1:],*([0]*(3-len(event)))))
 expected.append(w(0,*rec['final'],len(trace)//3,*trace,*([0]*(15-len(trace)))))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-recovery'],input=b''.join(commands))
assert len(actual)==len(records)*112
for i,want in enumerate(expected):assert actual[i*112:(i+1)*112]==want,('PC',i)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x10000);BE=B+0x1000;STACK=B+0xe000;STOP=B+0xf000;CB=[STOP+0x100,STOP+0x200,STOP+0x300]
entry=int(re.search(r'\s_rf_entity_ai_recover\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
cfg={};trace=[];calls=0;failure=-1

def hook(cpu,address,size,context):
 global calls
 if address not in CB:return
 calls+=1;sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);status=0
 if failure==calls:status=0xffffffff
 elif address==CB[0]:
  assert arg(1)==55;trace.extend((1,55,0));cpu.mem_write(arg(2),w(B if cfg['present'] else 0))
 elif address==CB[1]:
  assert arg(1)==77;trace.extend((2,77,0));cpu.mem_write(arg(2),w(cfg['linked']));cpu.mem_write(arg(3),w(cfg['health_bits']))
 else:
  assert arg(1)==B;op=arg(2)
  if op==0:trace.extend((3,r(B+24),0))
  elif op==1:trace.extend((4,40,0));cpu.mem_write(B+24,w(701,19))
  else:
   assert op==2;trace.extend((5,r(B+24),r(B+28)));cpu.mem_write(arg(3),struct.pack('<d',struct.unpack('<f',w(cfg['duration_bits']))[0]))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
def setup(rec):
 global cfg,trace,calls
 cfg=dict(rec['input']);x.mem_write(B,w(B,*rec['initial']));x.mem_write(BE,w(*CB,0));trace=[];calls=0
 x.mem_write(STACK,w(STOP,B,cfg['now'],BE));x.reg_write(UC_X86_REG_ESP,STACK)
def run():
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
for i,rec in enumerate(records):
 setup(rec);status=run();actual=w(status)+bytes(x.mem_read(B+4,44))+w(len(trace)//3,*trace,*([0]*(15-len(trace))))
 assert actual==expected[i],('NXDK',i)
rec=next(rec for rec in records if any(t[0]=='start' for t in rec['trace']) and any(t[0]=='linked' for t in rec['trace']))
for failure in range(1,6):
 setup(rec);assert run()==0xffffffff and calls==failure
failure=-1
# Invalid playback duration preserves both timers, while invalid class delay preserves only the second.
for bad in (0x7fc00000,0x7f800000,0xff800000,0x4f000000):
 setup(rec);cfg['duration_bits']=bad;before=bytes(x.mem_read(B+36,8));assert run()==0xfffffffc and bytes(x.mem_read(B+36,8))==before
 setup(rec);x.mem_write(B+44,w(bad));before=r(B+40);assert run()==0xfffffffc and r(B+36)==rec['timers'][0] and r(B+40)==before
report=dict(result='PASS',original_pc_nxdk_cases=len(records),callback_failures=5,duration_guards=8,scope='Full shared recovery, concrete gates/timers, exact retained fields and external trace versus original. Playback/lookup callbacks supplied, including changed model/motion. No scene integration or native XEMU claim.')
(root/'artifacts/ai-recovery-shared.json').write_text(json.dumps(report,indent=2));print(report)
